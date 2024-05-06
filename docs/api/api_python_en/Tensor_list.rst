.. role:: hidden
    :class: hidden-section

.. currentmodule:: {{ module }}

{% if objname in ["AdaSumByDeltaWeightWrapCell", "AdaSumByGradWrapCell", "DistributedGradReducer"] %}
{{ fullname | underline }}

.. autoclass:: {{ name }}
    :exclude-members: infer_value, infer_shape, infer_dtype, construct
    :members:

{% elif fullname in ["mindspore.nn.Adam","mindspore.nn.AdamWeightDecay","mindspore.nn.FTRL","mindspore.nn.LazyAdam","mindspore.nn.ProximalAdagrad"] %}
{{ fullname | underline }}

.. autoclass:: {{ name }}
    :exclude-members: infer_value, infer_shape, infer_dtype, target
    :members:

{% elif "mindspore.experimental.optim" in fullname and "lr_scheduler" not in fullname and objname[0].istitle() %}
{{ fullname | underline }}

.. autoclass:: {{ name }}
    :exclude-members: infer_value, infer_shape, infer_dtype, implementation
    :members:

{% elif fullname=="mindspore.Tensor" %}
{{ fullname | underline }}

.. autoclass:: {{ name }}

.. autosummary::
    :toctree: Tensor
    :nosignatures:

    mindspore.Tensor.abs
    mindspore.Tensor.absolute
    mindspore.Tensor.acos
    mindspore.Tensor.acosh
    mindspore.Tensor.add
    mindspore.Tensor.addbmm
    mindspore.Tensor.addcdiv
    mindspore.Tensor.addcmul
    mindspore.Tensor.addmm
    mindspore.Tensor.addmv
    mindspore.Tensor.addr
    mindspore.Tensor.adjoint
    mindspore.Tensor.all
    mindspore.Tensor.amax
    mindspore.Tensor.amin
    mindspore.Tensor.aminmax
    mindspore.Tensor.any
    mindspore.Tensor.angle
    mindspore.Tensor.approximate_equal
    mindspore.Tensor.arccos
    mindspore.Tensor.arccosh
    mindspore.Tensor.arcsin
    mindspore.Tensor.arcsinh
    mindspore.Tensor.arctan
    mindspore.Tensor.arctan2
    mindspore.Tensor.arctanh
    mindspore.Tensor.argmax
    mindspore.Tensor.argmax_with_value
    mindspore.Tensor.argmin
    mindspore.Tensor.argmin_with_value
    mindspore.Tensor.argsort
    mindspore.Tensor.argwhere
    mindspore.Tensor.asin
    mindspore.Tensor.asinh
    mindspore.Tensor.asnumpy
    mindspore.Tensor.assign_value
    mindspore.Tensor.astype
    mindspore.Tensor.atan
    mindspore.Tensor.atan2
    mindspore.Tensor.atanh
    mindspore.Tensor.baddbmm
    mindspore.Tensor.bernoulli
    mindspore.Tensor.bincount
    mindspore.Tensor.bitwise_and
    mindspore.Tensor.bitwise_left_shift
    mindspore.Tensor.bitwise_or
    mindspore.Tensor.bitwise_right_shift
    mindspore.Tensor.bitwise_xor
    mindspore.Tensor.bmm
    mindspore.Tensor.bool
    mindspore.Tensor.broadcast_to
    mindspore.Tensor.cauchy
    mindspore.Tensor.ceil
    mindspore.Tensor.cholesky
    mindspore.Tensor.cholesky_solve
    mindspore.Tensor.choose
    mindspore.Tensor.chunk
    mindspore.Tensor.clamp
    mindspore.Tensor.clip
    mindspore.Tensor.col2im
    mindspore.Tensor.conj
    mindspore.Tensor.contiguous
    mindspore.Tensor.copy
    mindspore.Tensor.copysign
    mindspore.Tensor.cos
    mindspore.Tensor.cosh
    mindspore.Tensor.count_nonzero
    mindspore.Tensor.cov
    mindspore.Tensor.cross
    mindspore.Tensor.cummax
    mindspore.Tensor.cummin
    mindspore.Tensor.cumprod
    mindspore.Tensor.cumsum
    mindspore.Tensor.deg2rad
    mindspore.Tensor.diag
    mindspore.Tensor.diagflat
    mindspore.Tensor.diagonal
    mindspore.Tensor.diagonal_scatter
    mindspore.Tensor.diff
    mindspore.Tensor.digamma
    mindspore.Tensor.div
    mindspore.Tensor.divide
    mindspore.Tensor.dot
    mindspore.Tensor.dsplit
    mindspore.Tensor.dtype
    mindspore.Tensor.eq
    mindspore.Tensor.equal
    mindspore.Tensor.erf
    mindspore.Tensor.erfc
    mindspore.Tensor.erfinv
    mindspore.Tensor.eigvals
    mindspore.Tensor.exp
    mindspore.Tensor.expand_as
    mindspore.Tensor.expand_dims
    mindspore.Tensor.expm1
    mindspore.Tensor.fill_diagonal
    mindspore.Tensor.flatten
    mindspore.Tensor.flip
    mindspore.Tensor.fliplr
    mindspore.Tensor.flipud
    mindspore.Tensor.float
    mindspore.Tensor.float_power
    mindspore.Tensor.floor
    mindspore.Tensor.floor_divide
    mindspore.Tensor.flush_from_cache
    mindspore.Tensor.fold
    mindspore.Tensor.fmax
    mindspore.Tensor.fmod
    mindspore.Tensor.frac
    mindspore.Tensor.from_numpy
    mindspore.Tensor.gather
    mindspore.Tensor.gather_elements
    mindspore.Tensor.gather_nd
    mindspore.Tensor.ge
    mindspore.Tensor.geqrf
    mindspore.Tensor.ger
    mindspore.Tensor.greater
    mindspore.Tensor.greater_equal
    mindspore.Tensor.gt
    mindspore.Tensor.H
    mindspore.Tensor.half
    mindspore.Tensor.hardshrink
    mindspore.Tensor.has_init
    mindspore.Tensor.heaviside
    mindspore.Tensor.histc
    mindspore.Tensor.hsplit
    mindspore.Tensor.hypot
    mindspore.Tensor.i0
    mindspore.Tensor.igamma
    mindspore.Tensor.igammac
    mindspore.Tensor.imag
    mindspore.Tensor.index_add
    mindspore.Tensor.index_fill
    mindspore.Tensor.index_put
    mindspore.Tensor.index_select
    mindspore.Tensor.init_data
    mindspore.Tensor.inner
    mindspore.Tensor.inplace_update
    mindspore.Tensor.int
    mindspore.Tensor.inv
    mindspore.Tensor.inverse
    mindspore.Tensor.invert
    mindspore.Tensor.isclose
    mindspore.Tensor.isfinite
    mindspore.Tensor.is_floating_point
    mindspore.Tensor.isinf
    mindspore.Tensor.isnan
    mindspore.Tensor.isneginf
    mindspore.Tensor.isposinf
    mindspore.Tensor.isreal
    mindspore.Tensor.is_signed
    mindspore.Tensor.is_complex
    mindspore.Tensor.is_contiguous
    mindspore.Tensor.item
    mindspore.Tensor.itemset
    mindspore.Tensor.itemsize
    mindspore.Tensor.lcm
    mindspore.Tensor.ldexp
    mindspore.Tensor.le
    mindspore.Tensor.lerp
    mindspore.Tensor.less
    mindspore.Tensor.less_equal
    mindspore.Tensor.log
    mindspore.Tensor.log10
    mindspore.Tensor.log1p
    mindspore.Tensor.log2
    mindspore.Tensor.logaddexp
    mindspore.Tensor.logaddexp2
    mindspore.Tensor.logcumsumexp
    mindspore.Tensor.logdet
    mindspore.Tensor.logical_and
    mindspore.Tensor.logical_not
    mindspore.Tensor.logical_or
    mindspore.Tensor.logical_xor
    mindspore.Tensor.logit
    mindspore.Tensor.logsumexp
    mindspore.Tensor.log_normal
    mindspore.Tensor.long
    mindspore.Tensor.lt
    mindspore.Tensor.lu_solve
    mindspore.Tensor.masked_fill
    mindspore.Tensor.masked_scatter
    mindspore.Tensor.masked_select
    mindspore.Tensor.matmul
    mindspore.Tensor.max
    mindspore.Tensor.maximum
    mindspore.Tensor.mean
    mindspore.Tensor.median
    mindspore.Tensor.mH
    mindspore.Tensor.min
    mindspore.Tensor.minimum
    mindspore.Tensor.mm
    mindspore.Tensor.movedim
    mindspore.Tensor.moveaxis
    mindspore.Tensor.msort
    mindspore.Tensor.mT
    mindspore.Tensor.mul
    mindspore.Tensor.multinomial
    mindspore.Tensor.multiply
    mindspore.Tensor.mvlgamma
    mindspore.Tensor.nan_to_num
    mindspore.Tensor.nanmean
    mindspore.Tensor.nanmedian
    mindspore.Tensor.nansum
    mindspore.Tensor.narrow
    mindspore.Tensor.nbytes
    mindspore.Tensor.ndim
    mindspore.Tensor.ndimension
    mindspore.Tensor.ne
    mindspore.Tensor.neg
    mindspore.Tensor.negative
    mindspore.Tensor.nelement
    mindspore.Tensor.new_ones
    mindspore.Tensor.new_zeros
    mindspore.Tensor.nextafter
    mindspore.Tensor.numel
    mindspore.Tensor.numpy
    mindspore.Tensor.nonzero
    mindspore.Tensor.norm
    mindspore.Tensor.not_equal
    mindspore.Tensor.outer
    mindspore.Tensor.orgqr
    mindspore.Tensor.ormqr
    mindspore.Tensor.permute
    mindspore.Tensor.positive
    mindspore.Tensor.pow
    mindspore.Tensor.prod
    mindspore.Tensor.ptp
    mindspore.Tensor.rad2deg
    mindspore.Tensor.random_categorical
    mindspore.Tensor.ravel
    mindspore.Tensor.real
    mindspore.Tensor.reciprocal
    mindspore.Tensor.remainder
    mindspore.Tensor.renorm
    mindspore.Tensor.repeat
    mindspore.Tensor.repeat_interleave
    mindspore.Tensor.reshape
    mindspore.Tensor.reshape_as
    mindspore.Tensor.resize
    mindspore.Tensor.reverse
    mindspore.Tensor.reverse_sequence
    mindspore.Tensor.roll
    mindspore.Tensor.round
    mindspore.Tensor.rot90
    mindspore.Tensor.rsqrt
    mindspore.Tensor.register_hook
    mindspore.Tensor.scatter
    mindspore.Tensor.scatter_add
    mindspore.Tensor.scatter_div
    mindspore.Tensor.scatter_max
    mindspore.Tensor.scatter_min
    mindspore.Tensor.scatter_mul
    mindspore.Tensor.scatter_sub
    mindspore.Tensor.searchsorted
    mindspore.Tensor.select
    mindspore.Tensor.select_scatter
    mindspore.Tensor.set_const_arg
    mindspore.Tensor.sgn
    mindspore.Tensor.shape
    mindspore.Tensor.short
    mindspore.Tensor.sigmoid
    mindspore.Tensor.sign
    mindspore.Tensor.signbit
    mindspore.Tensor.sin
    mindspore.Tensor.sinc
    mindspore.Tensor.sinh
    mindspore.Tensor.size
    mindspore.Tensor.slice_scatter
    mindspore.Tensor.slogdet
    mindspore.Tensor.softmax
    mindspore.Tensor.sort
    mindspore.Tensor.split
    mindspore.Tensor.sqrt
    mindspore.Tensor.square
    mindspore.Tensor.squeeze
    mindspore.Tensor.std
    mindspore.Tensor.storage_offset
    mindspore.Tensor.stride
    mindspore.Tensor.strides
    mindspore.Tensor.sub
    mindspore.Tensor.subtract
    mindspore.Tensor.sum
    mindspore.Tensor.sum_to_size
    mindspore.Tensor.svd
    mindspore.Tensor.swapaxes
    mindspore.Tensor.swapdims
    mindspore.Tensor.T
    mindspore.Tensor.t
    mindspore.Tensor.take
    mindspore.Tensor.tan
    mindspore.Tensor.tanh
    mindspore.Tensor.tensor_split
    mindspore.Tensor.tile
    mindspore.Tensor.to
    mindspore.Tensor.to_coo
    mindspore.Tensor.to_csr
    mindspore.Tensor.tolist
    mindspore.Tensor.topk
    mindspore.Tensor.trace
    mindspore.Tensor.transpose
    mindspore.Tensor.tril
    mindspore.Tensor.triu
    mindspore.Tensor.true_divide
    mindspore.Tensor.trunc
    mindspore.Tensor.type
    mindspore.Tensor.type_as
    mindspore.Tensor.unbind
    mindspore.Tensor.unfold
    mindspore.Tensor.unique_consecutive
    mindspore.Tensor.unique_with_pad
    mindspore.Tensor.unsorted_segment_max
    mindspore.Tensor.unsorted_segment_min
    mindspore.Tensor.unsorted_segment_prod
    mindspore.Tensor.unsqueeze
    mindspore.Tensor.var
    mindspore.Tensor.view
    mindspore.Tensor.view_as
    mindspore.Tensor.where
    mindspore.Tensor.xdivy
    mindspore.Tensor.xlogy
    mindspore.Tensor.vsplit

{% elif fullname=="mindspore.nn.Cell" %}
{{ fullname | underline }}

.. autoclass:: {{ name }}
    :exclude-members: infer_value, infer_shape, infer_dtype, auto_parallel_compile_and_run, load_parameter_slice, set_auto_parallel, set_parallel_input_with_inputs
    :members:

{% elif objname[0].istitle() %}
{{ fullname | underline }}

.. autoclass:: {{ name }}
    :exclude-members: infer_value, infer_shape, infer_dtype
    :members:

{% else %}
{{ fullname | underline }}

.. autofunction:: {{ fullname }}

{% endif %}

..
  autogenerated from _templates/classtemplate.rst
  note it does not have :inherited-members:
