from __future__ import absolute_import
import pytest
import numpy as np
import torch
import mindspore
from mindspore import nn
from mindspore import Tensor, context
from mindspore.experimental.optim import Rprop
from mindspore.experimental.optim.lr_scheduler import StepLR
from tests.st.utils import test_utils


class Network(nn.Cell):
    def __init__(self, lin_weight, lin_bias):
        super().__init__()
        self.lin = nn.Dense(2, 3, weight_init=lin_weight, bias_init=lin_bias)
        self.relu = nn.ReLU()

    def construct(self, x):
        out = self.lin(x)
        out = self.relu(out)
        return out


class NetworkPt(torch.nn.Module):
    def __init__(self):
        super().__init__()
        self.lin = torch.nn.Linear(2, 3)
        self.relu = torch.nn.ReLU()

    def forward(self, x):
        out = self.lin(x)
        out = self.relu(out)
        return out


class RpropFactory():
    def __init__(self, group=True, lr_dynamic=False, if_change=False, dtype=np.float32):
        super().__init__()
        np.random.seed(1024)
        self.lin_weight_np = np.random.randn(3, 2).astype(dtype)
        self.lin_bias_np = np.random.randn(3,).astype(dtype)

        self.group = group
        self.lr_dynamic = lr_dynamic
        self.if_change = if_change
        self.data = np.random.rand(2, 2).astype(np.float32)
        self.label = np.random.rand(2, 3).astype(np.float32)
        self.epochs = 1
        self.steps = 1
        self.lr = 0.002

        self.betas = (0.9, 0.999)
        self.eps = 1e-8
        self.weight_decay = 0
        self.amsgrad = False
        self.maximize = False

    def forward_pytorch_impl(self):
        lin_weight = torch.Tensor(self.lin_weight_np.copy())
        lin_bias = torch.Tensor(self.lin_bias_np.copy())

        model = NetworkPt()
        model.lin.weight = torch.nn.Parameter(lin_weight)
        model.lin.bias = torch.nn.Parameter(lin_bias)

        data = torch.from_numpy(self.data.copy())
        label = torch.from_numpy(self.label.copy())

        if not self.group:
            optimizer = torch.optim.Rprop(model.parameters(), self.lr)
        else:
            bias_params, no_bias_params = [], []
            for param in model.named_parameters():
                if "bias" in param[0]:
                    bias_params.append(param[1])
                else:
                    no_bias_params.append(param[1])

            group_params = [{'params': bias_params, 'etas': (0.5, 1.0)},
                            {'params': no_bias_params, 'lr': 0.66, "step_sizes": (1e-6, 48)}]
            optimizer = torch.optim.Rprop(params=group_params, lr=self.lr)

        criterion = torch.nn.L1Loss(reduction='mean')
        lr_scheduler = torch.optim.lr_scheduler.StepLR(optimizer, 2, gamma=0.5, last_epoch=-1)

        for _ in range(self.epochs):
            for _ in range(self.steps):
                optimizer.zero_grad()
                loss = criterion(model(data), label)
                loss.backward()
                optimizer.step()
            if self.lr_dynamic:
                lr_scheduler.step()
            if self.if_change:
                optimizer.param_groups[1]["etas"] = (0.3, 1.3)
        output = model(data)
        return output.detach().numpy()


    def forward_mindspore_impl(self):
        lin_weight = Tensor(self.lin_weight_np.copy())
        lin_bias = Tensor(self.lin_bias_np.copy())
        model = Network(lin_weight, lin_bias)

        data = Tensor(self.data)
        label = Tensor(self.label)

        if not self.group:
            optimizer = Rprop(model.trainable_params(), self.lr)
        else:
            bias_params = list(filter(lambda x: 'bias' in x.name, model.trainable_params()))
            no_bias_params = list(filter(lambda x: 'bias' not in x.name, model.trainable_params()))
            group_params = [{'params': bias_params, 'etas': (0.5, 1.0)},
                            {'params': no_bias_params, 'lr': 0.66, "step_sizes": (1e-6, 48)}]
            optimizer = Rprop(params=group_params, lr=self.lr)

        criterion = nn.MAELoss(reduction="mean")
        lr_scheduler = StepLR(optimizer, 2, gamma=0.5, last_epoch=-1)

        def forward_fn(data, label):
            logits = model(data)
            loss = criterion(logits, label)
            return loss, logits

        grad_fn = mindspore.value_and_grad(forward_fn, None, optimizer.parameters, has_aux=True)

        def train_step(data, label):
            (loss, _), grads = grad_fn(data, label)
            optimizer(grads)
            return loss

        def train(epochs, steps, lr_dynamic, if_change):
            for _ in range(epochs):
                for _ in range(steps):
                    train_step(data, label)
                if lr_dynamic:
                    lr_scheduler.step()
                if if_change:
                    optimizer.param_groups[1]["etas"] = (0.3, 1.3)

        train(self.epochs, self.steps, self.lr_dynamic, self.if_change)
        output = model(data)
        return output.asnumpy()

    def result_cmp(self):
        loss_expect = self.forward_pytorch_impl()
        loss_out = self.forward_mindspore_impl()
        allclose_nparray(loss_expect, loss_out, 0.005, 0.005)


def _count_unequal_element(data_expected, data_me, rtol, atol):
    assert data_expected.shape == data_me.shape
    total_count = len(data_expected.flatten())
    error = np.abs(data_expected - data_me)
    greater = np.greater(error, atol + np.abs(data_me) * rtol)
    loss_count = np.count_nonzero(greater)
    assert (loss_count / total_count) < rtol, \
        "\ndata_expected_std:{0}\ndata_me_error:{1}\nloss:{2}". \
            format(data_expected[greater], data_me[greater], error[greater])


def allclose_nparray(data_expected, data_me, rtol, atol, equal_nan=True):
    if np.any(np.isnan(data_expected)) or np.any(np.isnan(data_me)):
        assert np.allclose(data_expected, data_me, rtol, atol, equal_nan=equal_nan)
    elif not np.allclose(data_expected, data_me, rtol, atol, equal_nan=equal_nan):
        _count_unequal_element(data_expected, data_me, rtol, atol)
    else:
        assert np.array(data_expected).shape == np.array(data_me).shape


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
@test_utils.run_test_with_On
def test_rprop_basic(mode):
    """
    Feature: Test rprop.
    Description: Test rprop with default parameter.
    Expectation: success.
    """
    mindspore.set_context(mode=mode, jit_syntax_level=mindspore.STRICT)
    fact = RpropFactory(False, False)
    fact.result_cmp()


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_rprop_group(mode):
    """
    Feature: Test rprop.
    Description: Test rprop with grouped params.
    Expectation: success.
    """
    mindspore.set_context(mode=mode, jit_syntax_level=mindspore.STRICT)
    fact = RpropFactory(True, False)
    fact.result_cmp()


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_rprop_lr_dynamic(mode):
    """
    Feature: Test rprop.
    Description: Test rprop when lr is dynamic.
    Expectation: success.
    """
    mindspore.set_context(mode=mode, jit_syntax_level=mindspore.STRICT)
    fact = RpropFactory(False, True)
    fact.result_cmp()


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.GRAPH_MODE, context.PYNATIVE_MODE])
def test_rprop_group_lr_dynamic(mode):
    """
    Feature: Test rprop.
    Description: Test rprop with grouped params when lr is dynamic.
    Expectation: success.
    """
    mindspore.set_context(mode=mode, jit_syntax_level=mindspore.STRICT)
    fact = RpropFactory(True, True)
    fact.result_cmp()


@pytest.mark.level0
@pytest.mark.platform_x86_gpu_training
@pytest.mark.platform_arm_ascend_training
@pytest.mark.platform_x86_ascend_training
@pytest.mark.env_onecard
@pytest.mark.parametrize('mode', [context.PYNATIVE_MODE])
def test_rprop_group_lr_dynamic_change_param(mode):
    """
    Feature: Test rprop.
    Description: Test rprop with grouped params when optimizer params are changed.
    Expectation: success.
    """
    mindspore.set_context(mode=mode, jit_syntax_level=mindspore.STRICT)
    fact = RpropFactory(True, True, True)
    fact.result_cmp()
