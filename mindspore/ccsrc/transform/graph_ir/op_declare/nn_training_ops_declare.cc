/**
 * Copyright 2019-2021 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "transform/graph_ir/op_declare/nn_training_ops_declare.h"
#include <string>
#include "mindspore/core/ops/nn_optimizer_ops.h"
#include "ops/ascend_op_name.h"
#include "ops/nn_op_name.h"
#include "ops/nn_optimizer_op_name.h"

namespace mindspore::transform {
// ApplyMomentum
INPUT_MAP(ApplyMomentum) = {
  {1, INPUT_DESC(var)}, {2, INPUT_DESC(accum)}, {3, INPUT_DESC(lr)}, {4, INPUT_DESC(grad)}, {5, INPUT_DESC(momentum)}};
ATTR_MAP(ApplyMomentum) = {{"use_nesterov", ATTR_DESC(use_nesterov, AnyTraits<bool>())},
                           {"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())}};
OUTPUT_MAP(ApplyMomentum) = {{0, OUTPUT_DESC(var)}};
REG_ADPT_DESC(ApplyMomentum, kNameApplyMomentum, ADPT_DESC(ApplyMomentum))
REG_ADPT_DESC(ApplyMomentumD, prim::kPrimApplyMomentumD->name(), ADPT_DESC(ApplyMomentum))

// LarsV2Update
INPUT_MAP(LarsV2Update) = {{1, INPUT_DESC(w)},
                           {2, INPUT_DESC(g)},
                           {3, INPUT_DESC(w_square_sum)},
                           {4, INPUT_DESC(g_square_sum)},
                           {5, INPUT_DESC(weight_decay)},
                           {6, INPUT_DESC(learning_rate)}};
ATTR_MAP(LarsV2Update) = {{"epsilon", ATTR_DESC(epsilon, AnyTraits<float>())},
                          {"hyperpara", ATTR_DESC(hyperpara, AnyTraits<float>())},
                          {"use_clip", ATTR_DESC(use_clip, AnyTraits<bool>())}};
OUTPUT_MAP(LarsV2Update) = {{0, OUTPUT_DESC(g_new)}};
REG_ADPT_DESC(LARSUpdate, kNameLARSUpdate, ADPT_DESC(LarsV2Update))
REG_ADPT_DESC(LarsV2Update, kLarsV2UpdateOpName, ADPT_DESC(LarsV2Update))

// ApplyAdam
INPUT_MAP(ApplyAdam) = {{1, INPUT_DESC(var)},         {2, INPUT_DESC(m)},           {3, INPUT_DESC(v)},
                        {4, INPUT_DESC(beta1_power)}, {5, INPUT_DESC(beta2_power)}, {6, INPUT_DESC(lr)},
                        {7, INPUT_DESC(beta1)},       {8, INPUT_DESC(beta2)},       {9, INPUT_DESC(epsilon)},
                        {10, INPUT_DESC(grad)}};
ATTR_MAP(ApplyAdam) = {{"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())},
                       {"use_nesterov", ATTR_DESC(use_nesterov, AnyTraits<bool>())}};
OUTPUT_MAP(ApplyAdam) = {{0, OUTPUT_DESC(var)}};
REG_ADPT_DESC(ApplyAdam, kNameApplyAdam, ADPT_DESC(ApplyAdam))

// ApplyAdamD
INPUT_MAP(ApplyAdamD) = {{1, INPUT_DESC(var)},         {2, INPUT_DESC(m)},           {3, INPUT_DESC(v)},
                         {4, INPUT_DESC(beta1_power)}, {5, INPUT_DESC(beta2_power)}, {6, INPUT_DESC(lr)},
                         {7, INPUT_DESC(beta1)},       {8, INPUT_DESC(beta2)},       {9, INPUT_DESC(epsilon)},
                         {10, INPUT_DESC(grad)}};
ATTR_MAP(ApplyAdamD) = {{"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())},
                        {"use_nesterov", ATTR_DESC(use_nesterov, AnyTraits<bool>())}};
OUTPUT_MAP(ApplyAdamD) = {{0, OUTPUT_DESC(var)}, {1, OUTPUT_DESC(m)}, {2, OUTPUT_DESC(v)}};
REG_ADPT_DESC(Adam, kNameAdam, ADPT_DESC(ApplyAdamD))
REG_ADPT_DESC(ApplyAdamD, kNameApplyAdamD, ADPT_DESC(ApplyAdamD))

// ApplyAdagradD
INPUT_MAP(ApplyAdagradD) = {{1, INPUT_DESC(var)}, {2, INPUT_DESC(accum)}, {3, INPUT_DESC(lr)}, {4, INPUT_DESC(grad)}};
ATTR_MAP(ApplyAdagradD) = {{"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())}};
OUTPUT_MAP(ApplyAdagradD) = {{0, OUTPUT_DESC(var)}, {1, OUTPUT_DESC(accum)}};
REG_ADPT_DESC(ApplyAdagradD, kApplyAdagradDOpName, ADPT_DESC(ApplyAdagradD))
REG_ADPT_DESC(ApplyAdagrad, kApplyAdagradOpName, ADPT_DESC(ApplyAdagradD))

// ApplyAdagradV2D
INPUT_MAP(ApplyAdagradV2D) = {{1, INPUT_DESC(var)}, {2, INPUT_DESC(accum)}, {3, INPUT_DESC(lr)}, {4, INPUT_DESC(grad)}};
ATTR_MAP(ApplyAdagradV2D) = {{"epsilon", ATTR_DESC(epsilon, AnyTraits<float>())},
                             {"update_slots", ATTR_DESC(update_slots, AnyTraits<bool>())}};
OUTPUT_MAP(ApplyAdagradV2D) = {{0, OUTPUT_DESC(var)}, {1, OUTPUT_DESC(accum)}};
REG_ADPT_DESC(ApplyAdagradV2D, kNameApplyAdagradV2D, ADPT_DESC(ApplyAdagradV2D))
REG_ADPT_DESC(ApplyAdagradV2, kApplyAdagradV2OpName, ADPT_DESC(ApplyAdagradV2D))

// ApplyAddSignD
INPUT_MAP(ApplyAddSignD) = {{1, INPUT_DESC(var)},   {2, INPUT_DESC(m)},          {3, INPUT_DESC(lr)},
                            {4, INPUT_DESC(alpha)}, {5, INPUT_DESC(sign_decay)}, {6, INPUT_DESC(beta)},
                            {7, INPUT_DESC(grad)}};
ATTR_MAP(ApplyAddSignD) = EMPTY_ATTR_MAP;
OUTPUT_MAP(ApplyAddSignD) = {{0, OUTPUT_DESC(var)}, {1, OUTPUT_DESC(m)}};
REG_ADPT_DESC(ApplyAddSignD, kApplyAddSignDOpName, ADPT_DESC(ApplyAddSignD))
REG_ADPT_DESC(ApplyAddSign, kApplyAddSignOpName, ADPT_DESC(ApplyAddSignD))

// SparseApplyAdagradV2D
INPUT_MAP(SparseApplyAdagradV2D) = {
  {1, INPUT_DESC(var)}, {2, INPUT_DESC(accum)}, {3, INPUT_DESC(grad)}, {4, INPUT_DESC(indices)}};
ATTR_MAP(SparseApplyAdagradV2D) = {{"lr", ATTR_DESC(lr, AnyTraits<float>())},
                                   {"epsilon", ATTR_DESC(epsilon, AnyTraits<float>())},
                                   {"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())},
                                   {"update_slots", ATTR_DESC(update_slots, AnyTraits<bool>())}};
OUTPUT_MAP(SparseApplyAdagradV2D) = {{0, OUTPUT_DESC(var)}, {1, OUTPUT_DESC(accum)}};
REG_ADPT_DESC(SparseApplyAdagradV2D, kSparseApplyAdagradV2DOpName, ADPT_DESC(SparseApplyAdagradV2D))
REG_ADPT_DESC(SparseApplyAdagradV2, kSparseApplyAdagradV2OpName, ADPT_DESC(SparseApplyAdagradV2D))

// DataFormatDimMap
INPUT_MAP(DataFormatDimMap) = {{1, INPUT_DESC(x)}};
ATTR_MAP(DataFormatDimMap) = {{"dst_format", ATTR_DESC(src_format, AnyTraits<std::string>())},
                              {"src_format", ATTR_DESC(dst_format, AnyTraits<std::string>())}};
OUTPUT_MAP(DataFormatDimMap) = {{0, OUTPUT_DESC(y)}};
REG_ADPT_DESC(DataFormatDimMap, kNameDataFormatDimMap, ADPT_DESC(DataFormatDimMap))

// ApplyAdadeltaD
INPUT_MAP(ApplyAdadeltaD) = {{1, INPUT_DESC(var)}, {2, INPUT_DESC(accum)}, {3, INPUT_DESC(accum_update)},
                             {4, INPUT_DESC(lr)},  {5, INPUT_DESC(rho)},   {6, INPUT_DESC(epsilon)},
                             {7, INPUT_DESC(grad)}};
ATTR_MAP(ApplyAdadeltaD) = {{"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())}};
OUTPUT_MAP(ApplyAdadeltaD) = {{0, OUTPUT_DESC(var)}, {1, OUTPUT_DESC(accum)}, {2, OUTPUT_DESC(accum_update)}};
REG_ADPT_DESC(ApplyAdadeltaD, kNameApplyAdadelta, ADPT_DESC(ApplyAdadeltaD))

// ApplyAdaMaxD
INPUT_MAP(ApplyAdaMaxD) = {{1, INPUT_DESC(var)},         {2, INPUT_DESC(m)},       {3, INPUT_DESC(v)},
                           {4, INPUT_DESC(beta1_power)}, {5, INPUT_DESC(lr)},      {6, INPUT_DESC(beta1)},
                           {7, INPUT_DESC(beta2)},       {8, INPUT_DESC(epsilon)}, {9, INPUT_DESC(grad)}};
ATTR_MAP(ApplyAdaMaxD) = EMPTY_ATTR_MAP;
OUTPUT_MAP(ApplyAdaMaxD) = {{0, OUTPUT_DESC(var)}, {1, OUTPUT_DESC(m)}, {2, OUTPUT_DESC(v)}};
REG_ADPT_DESC(ApplyAdaMaxD, kApplyAdaMaxDOpName, ADPT_DESC(ApplyAdaMaxD))
REG_ADPT_DESC(ApplyAdaMax, kApplyAdaMaxOpName, ADPT_DESC(ApplyAdaMaxD))

// ApplyGradientDescent
INPUT_MAP(ApplyGradientDescent) = {{1, INPUT_DESC(var)}, {2, INPUT_DESC(alpha)}, {3, INPUT_DESC(delta)}};
ATTR_MAP(ApplyGradientDescent) = {{"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())}};
OUTPUT_MAP(ApplyGradientDescent) = {{0, OUTPUT_DESC(var)}};
REG_ADPT_DESC(ApplyGradientDescent, kNameApplyGradientDescent, ADPT_DESC(ApplyGradientDescent))

// ApplyPowerSignD
INPUT_MAP(ApplyPowerSignD) = {{1, INPUT_DESC(var)},     {2, INPUT_DESC(m)},          {3, INPUT_DESC(lr)},
                              {4, INPUT_DESC(logbase)}, {5, INPUT_DESC(sign_decay)}, {6, INPUT_DESC(beta)},
                              {7, INPUT_DESC(grad)}};
ATTR_MAP(ApplyPowerSignD) = EMPTY_ATTR_MAP;
OUTPUT_MAP(ApplyPowerSignD) = {{0, OUTPUT_DESC(var)}, {1, OUTPUT_DESC(m)}};
REG_ADPT_DESC(ApplyPowerSignD, kNameApplyPowerSignD, ADPT_DESC(ApplyPowerSignD))
REG_ADPT_DESC(ApplyPowerSign, kApplyPowerSignOpName, ADPT_DESC(ApplyPowerSignD))

// ApplyProximalGradientDescent
INPUT_MAP(ApplyProximalGradientDescent) = {
  {1, INPUT_DESC(var)}, {2, INPUT_DESC(alpha)}, {3, INPUT_DESC(l1)}, {4, INPUT_DESC(l2)}, {5, INPUT_DESC(delta)}};
ATTR_MAP(ApplyProximalGradientDescent) = {{"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())}};
OUTPUT_MAP(ApplyProximalGradientDescent) = {{0, OUTPUT_DESC(var)}};
REG_ADPT_DESC(ApplyProximalGradientDescent, kNameApplyProximalGradientDescent, ADPT_DESC(ApplyProximalGradientDescent))

// SGD
INPUT_MAP(SGD) = {{1, INPUT_DESC(parameters)}, {2, INPUT_DESC(gradient)}, {3, INPUT_DESC(learning_rate)},
                  {4, INPUT_DESC(accum)},      {5, INPUT_DESC(momentum)}, {6, INPUT_DESC(stat)}};
ATTR_MAP(SGD) = {{"dampening", ATTR_DESC(dampening, AnyTraits<float>())},
                 {"weight_decay", ATTR_DESC(weight_decay, AnyTraits<float>())},
                 {"nesterov", ATTR_DESC(nesterov, AnyTraits<bool>())}};
OUTPUT_MAP(SGD) = {{0, OUTPUT_DESC(parameters)}};
REG_ADPT_DESC(SGD, kNameSGD, ADPT_DESC(SGD))

// SparseApplyAdagradD
INPUT_MAP(SparseApplyAdagradD) = {
  {1, INPUT_DESC(var)}, {2, INPUT_DESC(accum)}, {3, INPUT_DESC(grad)}, {4, INPUT_DESC(indices)}};
ATTR_MAP(SparseApplyAdagradD) = {{"lr", ATTR_DESC(lr, AnyTraits<float>())},
                                 {"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())},
                                 {"update_slots", ATTR_DESC(update_slots, AnyTraits<bool>())}};
OUTPUT_MAP(SparseApplyAdagradD) = {{0, OUTPUT_DESC(var)}, {1, OUTPUT_DESC(accum)}};
REG_ADPT_DESC(SparseApplyAdagradD, kNameSparseApplyAdagrad, ADPT_DESC(SparseApplyAdagradD))
REG_ADPT_DESC(SparseApplyAdagrad, kNameSparseApplyAdagradD, ADPT_DESC(SparseApplyAdagradD))

// ApplyProximalAdagradD
INPUT_MAP(ApplyProximalAdagradD) = {{1, INPUT_DESC(var)}, {2, INPUT_DESC(accum)}, {3, INPUT_DESC(lr)},
                                    {4, INPUT_DESC(l1)},  {5, INPUT_DESC(l2)},    {6, INPUT_DESC(grad)}};
ATTR_MAP(ApplyProximalAdagradD) = {{"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())}};
OUTPUT_MAP(ApplyProximalAdagradD) = {{0, OUTPUT_DESC(var)}, {1, OUTPUT_DESC(accum)}};
REG_ADPT_DESC(ApplyProximalAdagradD, kNameApplyProximalAdagrad, ADPT_DESC(ApplyProximalAdagradD))

// SparseApplyProximalAdagradD
INPUT_MAP(SparseApplyProximalAdagradD) = {{1, INPUT_DESC(var)},    {2, INPUT_DESC(accum)}, {3, INPUT_DESC(lr)},
                                          {4, INPUT_DESC(l1)},     {5, INPUT_DESC(l2)},    {6, INPUT_DESC(grad)},
                                          {7, INPUT_DESC(indices)}};
ATTR_MAP(SparseApplyProximalAdagradD) = {{"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())}};
OUTPUT_MAP(SparseApplyProximalAdagradD) = {{0, OUTPUT_DESC(var)}, {1, OUTPUT_DESC(accum)}};
REG_ADPT_DESC(SparseApplyProximalAdagradD, kSparseApplyProximalAdagradDOpName, ADPT_DESC(SparseApplyProximalAdagradD))
REG_ADPT_DESC(SparseApplyProximalAdagrad, kNameSparseApplyProximalAdagrad, ADPT_DESC(SparseApplyProximalAdagradD))

// SparseApplyFtrlD
INPUT_MAP(SparseApplyFtrlD) = {{1, INPUT_DESC(var)},
                               {2, INPUT_DESC(accum)},
                               {3, INPUT_DESC(linear)},
                               {4, INPUT_DESC(grad)},
                               {5, INPUT_DESC(indices)}};
ATTR_MAP(SparseApplyFtrlD) = {{"lr", ATTR_DESC(lr, AnyTraits<float>())},
                              {"l1", ATTR_DESC(l1, AnyTraits<float>())},
                              {"l2", ATTR_DESC(l2, AnyTraits<float>())},
                              {"lr_power", ATTR_DESC(lr_power, AnyTraits<float>())},
                              {"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())}};
OUTPUT_MAP(SparseApplyFtrlD) = {{0, OUTPUT_DESC(var)}, {1, OUTPUT_DESC(accum)}, {2, OUTPUT_DESC(linear)}};
REG_ADPT_DESC(SparseApplyFtrlD, kNameSparseApplyFtrlD, ADPT_DESC(SparseApplyFtrlD))
REG_ADPT_DESC(SparseApplyFtrl, kSparseApplyFtrlOpName, ADPT_DESC(SparseApplyFtrlD))

// SparseApplyFtrlV2D
INPUT_MAP(SparseApplyFtrlV2D) = {{1, INPUT_DESC(var)},
                                 {2, INPUT_DESC(accum)},
                                 {3, INPUT_DESC(linear)},
                                 {4, INPUT_DESC(grad)},
                                 {5, INPUT_DESC(indices)}};
ATTR_MAP(SparseApplyFtrlV2D) = {{"lr", ATTR_DESC(lr, AnyTraits<float>())},
                                {"l1", ATTR_DESC(l1, AnyTraits<float>())},
                                {"l2", ATTR_DESC(l2, AnyTraits<float>())},
                                {"l2_shrinkage", ATTR_DESC(l2_shrinkage, AnyTraits<float>())},
                                {"lr_power", ATTR_DESC(lr_power, AnyTraits<float>())},
                                {"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())}};
OUTPUT_MAP(SparseApplyFtrlV2D) = {{0, OUTPUT_DESC(var)}, {1, OUTPUT_DESC(accum)}, {2, OUTPUT_DESC(linear)}};
REG_ADPT_DESC(SparseApplyFtrlV2D, kSparseApplyFtrlV2DOpName, ADPT_DESC(SparseApplyFtrlV2D))
REG_ADPT_DESC(SparseApplyFtrlV2, kSparseApplyFtrlV2OpName, ADPT_DESC(SparseApplyFtrlV2D))

// ApplyFtrl
INPUT_MAP(ApplyFtrl) = {{1, INPUT_DESC(var)},  {2, INPUT_DESC(accum)},   {3, INPUT_DESC(linear)},
                        {4, INPUT_DESC(grad)}, {5, INPUT_DESC(lr)},      {6, INPUT_DESC(l1)},
                        {7, INPUT_DESC(l2)},   {8, INPUT_DESC(lr_power)}};
ATTR_MAP(ApplyFtrl) = {{"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())}};
OUTPUT_MAP(ApplyFtrl) = {{0, OUTPUT_DESC(var)}};
REG_ADPT_DESC(ApplyFtrl, kNameApplyFtrl, ADPT_DESC(ApplyFtrl))
REG_ADPT_DESC(ApplyFtrlD, prim::kPrimApplyFtrlD->name(), ADPT_DESC(ApplyFtrl))

// ApplyRMSPropD
INPUT_MAP(ApplyRMSPropD) = {
  {1, INPUT_DESC(var)}, {2, INPUT_DESC(ms)}, {3, INPUT_DESC(mom)}, {4, INPUT_DESC(lr)}, {5, INPUT_DESC(grad)}};
INPUT_ATTR_MAP(ApplyRMSPropD) = {{6, ATTR_DESC(rho, AnyTraits<float>())},
                                 {7, ATTR_DESC(momentum, AnyTraits<float>())},
                                 {8, ATTR_DESC(epsilon, AnyTraits<float>())}};
ATTR_MAP(ApplyRMSPropD) = {{"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())}};
OUTPUT_MAP(ApplyRMSPropD) = {{0, OUTPUT_DESC(var)}, {1, OUTPUT_DESC(ms)}, {2, OUTPUT_DESC(mom)}};
REG_ADPT_DESC(ApplyRMSPropD, kNameApplyRMSProp, ADPT_DESC(ApplyRMSPropD))

// ApplyCenteredRMSProp
INPUT_MAP(ApplyCenteredRMSProp) = {{1, INPUT_DESC(var)}, {2, INPUT_DESC(mg)},       {3, INPUT_DESC(ms)},
                                   {4, INPUT_DESC(mom)}, {5, INPUT_DESC(grad)},     {6, INPUT_DESC(lr)},
                                   {7, INPUT_DESC(rho)}, {8, INPUT_DESC(momentum)}, {9, INPUT_DESC(epsilon)}};
ATTR_MAP(ApplyCenteredRMSProp) = {{"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())}};
OUTPUT_MAP(ApplyCenteredRMSProp) = {{0, OUTPUT_DESC(var)}};
REG_ADPT_DESC(ApplyCenteredRMSProp, kNameApplyCenteredRMSProp, ADPT_DESC(ApplyCenteredRMSProp))

// SparseApplyRMSProp
INPUT_MAP(SparseApplyRMSProp) = {{1, INPUT_DESC(var)},     {2, INPUT_DESC(ms)},   {3, INPUT_DESC(mom)},
                                 {4, INPUT_DESC(lr)},      {5, INPUT_DESC(rho)},  {6, INPUT_DESC(momentum)},
                                 {7, INPUT_DESC(epsilon)}, {8, INPUT_DESC(grad)}, {9, INPUT_DESC(indices)}};
ATTR_INPUT_MAP(SparseApplyRMSProp) = {{"rho", "rho"}, {"momentum", "momentum"}, {"epsilon", "epsilon"}};
ATTR_MAP(SparseApplyRMSProp) = {{"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())}};
OUTPUT_MAP(SparseApplyRMSProp) = {{0, OUTPUT_DESC(var)}};

// SparseApplyRMSPropD
INPUT_MAP(SparseApplyRMSPropD) = {{1, INPUT_DESC(var)}, {2, INPUT_DESC(ms)},   {3, INPUT_DESC(mom)},
                                  {4, INPUT_DESC(lr)},  {5, INPUT_DESC(grad)}, {6, INPUT_DESC(indices)}};
ATTR_MAP(SparseApplyRMSPropD) = {{"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())},
                                 {"rho", ATTR_DESC(rho, AnyTraits<float>())},
                                 {"momentum", ATTR_DESC(momentum, AnyTraits<float>())},
                                 {"epsilon", ATTR_DESC(epsilon, AnyTraits<float>())}};
OUTPUT_MAP(SparseApplyRMSPropD) = {{0, OUTPUT_DESC(var)}, {1, OUTPUT_DESC(ms)}, {2, OUTPUT_DESC(mom)}};
REG_ADPT_DESC(SparseApplyRMSPropD, prim::kPrimSparseApplyRMSProp->name(), ADPT_DESC(SparseApplyRMSPropD))

// SparseApplyAdagrad
INPUT_MAP(SparseApplyAdagrad) = {
  {1, INPUT_DESC(var)}, {2, INPUT_DESC(accum)}, {3, INPUT_DESC(grad)}, {4, INPUT_DESC(indices)}, {5, INPUT_DESC(lr)}};
ATTR_INPUT_MAP(SparseApplyAdagrad) = {{"lr", "lr"}};
ATTR_MAP(SparseApplyAdagrad) = {{"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())},
                                {"update_slots", ATTR_DESC(update_slots, AnyTraits<bool>())}};
OUTPUT_MAP(SparseApplyAdagrad) = {{0, OUTPUT_DESC(var)}, {1, OUTPUT_DESC(accum)}};

// ApplyKerasMomentumD
INPUT_MAP(ApplyKerasMomentumD) = {
  {1, INPUT_DESC(var)}, {2, INPUT_DESC(accum)}, {3, INPUT_DESC(lr)}, {4, INPUT_DESC(grad)}, {5, INPUT_DESC(momentum)}};
ATTR_MAP(ApplyKerasMomentumD) = {{"use_nesterov", ATTR_DESC(use_nesterov, AnyTraits<bool>())},
                                 {"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())}};
OUTPUT_MAP(ApplyKerasMomentumD) = {{0, OUTPUT_DESC(var)}, {1, OUTPUT_DESC(accum)}};
REG_ADPT_DESC(ApplyKerasMomentumD, kApplyKerasMomentumDOpName, ADPT_DESC(ApplyKerasMomentumD))
REG_ADPT_DESC(ApplyKerasMomentum, kApplyKerasMomentumOpName, ADPT_DESC(ApplyKerasMomentumD))

// ApplyAdamWithAmsgradV2
INPUT_MAP(ApplyAdamWithAmsgradV2) = {
  {1, INPUT_DESC(var)},         {2, INPUT_DESC(m)},           {3, INPUT_DESC(v)},    {4, INPUT_DESC(vhat)},
  {5, INPUT_DESC(beta1_power)}, {6, INPUT_DESC(beta2_power)}, {7, INPUT_DESC(lr)},   {8, INPUT_DESC(beta1)},
  {9, INPUT_DESC(beta2)},       {10, INPUT_DESC(epsilon)},    {11, INPUT_DESC(grad)}};
ATTR_INPUT_MAP(ApplyAdamWithAmsgradV2) = {{"beta1", "beta1"}, {"beta2", "beta2"}, {"epsilon", "epsilon"}};
ATTR_MAP(ApplyAdamWithAmsgradV2) = {{"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())}};
OUTPUT_MAP(ApplyAdamWithAmsgradV2) = {
  {0, OUTPUT_DESC(var)}, {1, OUTPUT_DESC(m)}, {2, OUTPUT_DESC(v)}, {3, OUTPUT_DESC(vhat)}};
REG_ADPT_DESC(ApplyAdamWithAmsgradV2, kApplyAdamWithAmsgradV2OpName, ADPT_DESC(ApplyAdamWithAmsgradV2))

// ApplyAdamWithAmsgradD
INPUT_MAP(ApplyAdamWithAmsgradD) = {{1, INPUT_DESC(var)},  {2, INPUT_DESC(m)},           {3, INPUT_DESC(v)},
                                    {4, INPUT_DESC(vhat)}, {5, INPUT_DESC(beta1_power)}, {6, INPUT_DESC(beta2_power)},
                                    {7, INPUT_DESC(lr)},   {8, INPUT_DESC(grad)}};
ATTR_MAP(ApplyAdamWithAmsgradD) = {{"beta1", ATTR_DESC(beta1, AnyTraits<float>())},
                                   {"beta2", ATTR_DESC(beta2, AnyTraits<float>())},
                                   {"epsilon", ATTR_DESC(epsilon, AnyTraits<float>())},
                                   {"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())}};
OUTPUT_MAP(ApplyAdamWithAmsgradD) = {
  {0, OUTPUT_DESC(var)}, {1, OUTPUT_DESC(m)}, {2, OUTPUT_DESC(v)}, {3, OUTPUT_DESC(vhat)}};
REG_ADPT_DESC(ApplyAdamWithAmsgradD, kApplyAdamWithAmsgradDOpName, ADPT_DESC(ApplyAdamWithAmsgradD));
REG_ADPT_DESC(ApplyAdamWithAmsgrad, kApplyAdamWithAmsgradOpName, ADPT_DESC(ApplyAdamWithAmsgradD))

// ApplyAdagradDA
INPUT_MAP(ApplyAdagradDA) = {{1, INPUT_DESC(var)},
                             {2, INPUT_DESC(gradient_accumulator)},
                             {3, INPUT_DESC(gradient_squared_accumulator)},
                             {4, INPUT_DESC(grad)},
                             {5, INPUT_DESC(lr)},
                             {6, INPUT_DESC(l1)},
                             {7, INPUT_DESC(l2)},
                             {8, INPUT_DESC(global_step)}};
ATTR_MAP(ApplyAdagradDA) = {{"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())}};
OUTPUT_MAP(ApplyAdagradDA) = {{0, OUTPUT_DESC(var)}};
REG_ADPT_DESC(ApplyAdagradDA, kApplyAdagradDAOpName, ADPT_DESC(ApplyAdagradDA))

// ApplyRMSProp
INPUT_MAP(ApplyRMSProp) = {
  {1, INPUT_DESC(var)}, {2, INPUT_DESC(ms)},       {3, INPUT_DESC(mom)},     {4, INPUT_DESC(lr)},
  {6, INPUT_DESC(rho)}, {7, INPUT_DESC(momentum)}, {8, INPUT_DESC(epsilon)}, {5, INPUT_DESC(grad)},
};
ATTR_MAP(ApplyRMSProp) = {{"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())}};
OUTPUT_MAP(ApplyRMSProp) = {{0, OUTPUT_DESC(var)}};
REG_ADPT_DESC(ApplyRMSProp, kApplyRMSPropOpName, ADPT_DESC(ApplyRMSProp))

// ApplyProximalAdagrad
INPUT_MAP(ApplyProximalAdagrad) = {{1, INPUT_DESC(var)}, {2, INPUT_DESC(accum)}, {3, INPUT_DESC(lr)},
                                   {4, INPUT_DESC(l1)},  {5, INPUT_DESC(l2)},    {6, INPUT_DESC(grad)}};
ATTR_MAP(ApplyProximalAdagrad) = {{"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())}};
OUTPUT_MAP(ApplyProximalAdagrad) = {{0, OUTPUT_DESC(var)}};
REG_ADPT_DESC(ApplyProximalAdagrad, kApplyProximalAdagradDOpName, ADPT_DESC(ApplyProximalAdagrad))

// SparseApplyProximalAdagrad
INPUT_MAP(SparseApplyProximalAdagrad) = {{1, INPUT_DESC(var)},    {2, INPUT_DESC(accum)}, {3, INPUT_DESC(lr)},
                                         {4, INPUT_DESC(l1)},     {5, INPUT_DESC(l2)},    {6, INPUT_DESC(grad)},
                                         {7, INPUT_DESC(indices)}};
ATTR_MAP(SparseApplyProximalAdagrad) = {{"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())}};
OUTPUT_MAP(SparseApplyProximalAdagrad) = {{0, OUTPUT_DESC(var)}};

// ApplyAdadelta
INPUT_MAP(ApplyAdadelta) = {{1, INPUT_DESC(var)}, {2, INPUT_DESC(accum)}, {3, INPUT_DESC(accum_update)},
                            {4, INPUT_DESC(lr)},  {5, INPUT_DESC(rho)},   {6, INPUT_DESC(epsilon)},
                            {7, INPUT_DESC(grad)}};
ATTR_MAP(ApplyAdadelta) = {{"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())}};
OUTPUT_MAP(ApplyAdadelta) = {{0, OUTPUT_DESC(var)}};
REG_ADPT_DESC(ApplyAdadelta, kApplyAdadeltaDOpName, ADPT_DESC(ApplyAdadelta))

// SparseApplyAdadelta
INPUT_MAP(SparseApplyAdadelta) = {{1, INPUT_DESC(var)},  {2, INPUT_DESC(accum)},  {3, INPUT_DESC(accum_update)},
                                  {4, INPUT_DESC(lr)},   {5, INPUT_DESC(rho)},    {6, INPUT_DESC(epsilon)},
                                  {7, INPUT_DESC(grad)}, {8, INPUT_DESC(indices)}};
ATTR_INPUT_MAP(SparseApplyAdadelta) = {{"epsilon", "epsilon"}};
ATTR_MAP(SparseApplyAdadelta) = {{"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())}};
OUTPUT_MAP(SparseApplyAdadelta) = {{0, OUTPUT_DESC(var)}};
REG_ADPT_DESC(SparseApplyAdadelta, kSparseApplyAdadeltaDOpName, ADPT_DESC(SparseApplyAdadelta))

// FusedSparseProximalAdagrad
CUST_INPUT_MAP(FusedSparseProximalAdagrad) = {{1, INPUT_DESC(var)},    {2, INPUT_DESC(accum)}, {3, INPUT_DESC(lr)},
                                              {4, INPUT_DESC(l1)},     {5, INPUT_DESC(l2)},    {6, INPUT_DESC(grad)},
                                              {7, INPUT_DESC(indices)}};
CUST_ATTR_MAP(FusedSparseProximalAdagrad) = {{"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())}};
CUST_OUTPUT_MAP(FusedSparseProximalAdagrad) = {{0, OUTPUT_DESC(var)}, {1, OUTPUT_DESC(accum)}};
REG_ADPT_DESC(FusedSparseProximalAdagrad, prim::kPrimFusedSparseProximalAdagrad->name(),
              CUST_ADPT_DESC(FusedSparseProximalAdagrad));

// FusedSparseFtrl
CUST_INPUT_MAP(FusedSparseFtrl) = {{1, INPUT_DESC(var)},
                                   {2, INPUT_DESC(accum)},
                                   {3, INPUT_DESC(linear)},
                                   {4, INPUT_DESC(grad)},
                                   {5, INPUT_DESC(indices)}};
CUST_ATTR_MAP(FusedSparseFtrl) = {{"lr", ATTR_DESC(lr, AnyTraits<float>())},
                                  {"l1", ATTR_DESC(l1, AnyTraits<float>())},
                                  {"l2", ATTR_DESC(l2, AnyTraits<float>())},
                                  {"lr_power", ATTR_DESC(lr_power, AnyTraits<float>())},
                                  {"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())}};
CUST_OUTPUT_MAP(FusedSparseFtrl) = {{0, OUTPUT_DESC(var)}, {1, OUTPUT_DESC(accum)}, {2, OUTPUT_DESC(linear)}};
REG_ADPT_DESC(FusedSparseFtrl, prim::kPrimFusedSparseFtrl->name(), CUST_ADPT_DESC(FusedSparseFtrl));

// FusedSparseAdam
CUST_INPUT_MAP(FusedSparseAdam) = {{1, INPUT_DESC(var)},         {2, INPUT_DESC(m)},           {3, INPUT_DESC(v)},
                                   {4, INPUT_DESC(beta1_power)}, {5, INPUT_DESC(beta2_power)}, {6, INPUT_DESC(lr)},
                                   {7, INPUT_DESC(beta1)},       {8, INPUT_DESC(beta2)},       {9, INPUT_DESC(epsilon)},
                                   {10, INPUT_DESC(grad)},       {11, INPUT_DESC(indices)}};
CUST_ATTR_MAP(FusedSparseAdam) = {{"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())},
                                  {"use_nesterov", ATTR_DESC(use_nesterov, AnyTraits<bool>())}};
CUST_OUTPUT_MAP(FusedSparseAdam) = {{0, OUTPUT_DESC(var)}, {1, OUTPUT_DESC(m)}, {2, OUTPUT_DESC(v)}};
REG_ADPT_DESC(FusedSparseAdam, prim::kPrimFusedSparseAdam->name(), CUST_ADPT_DESC(FusedSparseAdam));

// FusedSparseLazyAdam
CUST_INPUT_MAP(FusedSparseLazyAdam) = {
  {1, INPUT_DESC(var)},         {2, INPUT_DESC(m)},     {3, INPUT_DESC(v)},       {4, INPUT_DESC(beta1_power)},
  {5, INPUT_DESC(beta2_power)}, {6, INPUT_DESC(lr)},    {7, INPUT_DESC(beta1)},   {8, INPUT_DESC(beta2)},
  {9, INPUT_DESC(epsilon)},     {10, INPUT_DESC(grad)}, {11, INPUT_DESC(indices)}};
CUST_ATTR_MAP(FusedSparseLazyAdam) = {{"use_locking", ATTR_DESC(use_locking, AnyTraits<bool>())},
                                      {"use_nesterov", ATTR_DESC(use_nesterov, AnyTraits<bool>())}};
CUST_OUTPUT_MAP(FusedSparseLazyAdam) = {{0, OUTPUT_DESC(var)}, {1, OUTPUT_DESC(m)}, {2, OUTPUT_DESC(v)}};
REG_ADPT_DESC(FusedSparseLazyAdam, prim::kPrimFusedSparseLazyAdam->name(), CUST_ADPT_DESC(FusedSparseLazyAdam));
}  // namespace mindspore::transform
