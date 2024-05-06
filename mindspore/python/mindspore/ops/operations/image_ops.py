# Copyright 2020-2022 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ============================================================================

"""image_ops"""

from __future__ import absolute_import
from mindspore import context
from mindspore import _checkparam as validator
from mindspore.ops.primitive import prim_attr_register, Primitive
from mindspore.common import dtype as mstype
from ..auto_generate import ResizeLinear1D, ResizeBilinearV2, ResizeBicubic


class AdjustSaturation(Primitive):
    """
    Adjust saturation of RGB images.

    Note:
        This is a convenience method that converts RGB images to float representation, converts them to HSV,
        adds an offset to the saturation channel, converts back to RGB and then back to the original data type.
        If several adjustments are chained it is advisable to minimize the number of redundant conversions.

    Inputs:
        - **image** (Tensor) - Images to adjust. Must be one of the following types: float16, float32.
          At least 3-D. The last dimension is interpreted as channels, and must be three.
        - **scale** (Tensor) - A scale factor determines the amount of saturation adjustment to
          apply to the image. A value greater than 1.0 increases the saturation, while a value less than
          1.0 decreases the saturation. A value of 1.0 leaves the saturation unchanged.
          Must be 0-D Tensor of type float32.

    Outputs:
        Adjusted image(s), same shape and dtype as `image`.

    Raises:
        TypeError: If any iput is not Tensor.
        TypeError: If the type of `image` is not one of the following dtype: float16, float32.
        TypeError: If the type of `scale` is not float32.
        ValueError: If the dimension of the 'image' is less than 3.
        ValueError: If the last dimension of the 'image' is not 3.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
      >>> x = Tensor([[[1.0, 2.0, 3.0],
      ...       [4.0, 5.0, 6.0]],
      ...     [[7.0, 8.0, 9.0],
      ...       [10.0, 11.0, 12.0]]])
      >>> scale = Tensor(float(0.5))
      >>> adjustsaturation = ops.AdjustSaturation()
      >>> output = adjustsaturation(x, scale)
      >>> print(output)
             [[[ 2.         2.4999998  3.       ]
          [ 5.         5.5        6.       ]]
         [[ 8.         8.5        9.       ]
          [11.        11.5       12.       ]]]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize AdjustSaturation"""
        self.init_prim_io_names(inputs=['images', 'scale'], outputs=['y'])


class AdjustContrastv2(Primitive):
    """
    Adjust contrastv2 of images.

    Note:
        images is a tensor of at least 3 dimensions.
        The last 3 dimensions are interpreted as [height, width, channels].
        The other dimensions only represent a collection of images, such as [batch, height, width, channels].
        Contrast is adjusted independently for each channel of each image.

    Inputs:
        -**images**(tensor): Images to adjust. Must be one of the following types: float16, float32.
          At least 3-D.The last dimension is interpreted as channels, and must be three.
        -**contrast_factor**(tensor): A float multiplier for adjusting contrast. A Tensor of type float32. Must be 0-D.

    Outputs:
        Adjusted image(s), same shape and dtype as `images`.

    Raises:
        TypeError: If any input is not Tensor.
        TypeError: If the type of `images` is not one of the following dtype: float16, float32.
        TypeError: If the type of `contrast_factor` is not float32.
        ValueError: If the dimension of the 'images' is less than 3, or the last dimension of the 'images' is not 3.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
    >>> images = Tensor([[[1.0, 2.0, 3.0],
    ...       [4.0, 5.0, 6.0]],
    ...     [[7.0, 8.0, 9.0],
    ...       [10.0, 11.0, 12.0]]], mstype.float32)
    >>> contrast_factor = Tensor(2., mstype.float32)
    >>> adjustcontrastv2 = AdjustContrastv2()
    >>> output = adjustcontrastv2(images, contrast_factor)
    >>> print(output)
    [[[-3.5 -2.5 -1.5]
      [ 2.5  3.5  4.5]]
    <BLANKLINE>
     [[ 8.5  9.5 10.5]
      [14.5 15.5 16.5]]]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize AdjustContrastv2"""
        self.init_prim_io_names(inputs=['images', 'contrast_factor'], outputs=['y'])


class AdjustHue(Primitive):
    """
    Adjust hue of RGB images.

    Note:
        A convenience method that transform an RGB image to float representation.
        The image is adjusted by transforming the image to HSV and shifting the intensities in the hue channel,
        then transform back to original data mode.
        It is recommended to minimize the number of redundant transformations when several adjustments are chained.

    Inputs:
        - **image** (Tensor): RGB image or images, a Tensor has at least 3-D.
          The last dimension is interpreted as channels whose size must be three.
          the dtype is float16 or float32.
        - **delta** (Tensor): How much to add to the hue channel, the dtype is float32. Must be 0-D.

    Outputs:
        Adjusted image(s), same shape and dtype as `image`.

    Raises:
        TypeError: If neither `image` nor `delta` is a tensor.
        TypeError: If the dtype of `image` is neither float16 nor float32.
        TypeError: If the dtype of `delta` not float32.
        ValueError: If the dimension of `image` is less than 3.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
         >>> class AdjustHue(nn.Cell):
         ...   def __init__(self):
         ...     super(AdjustHue, self).__init__()
         ...     self.adjustHue = ops.AdjustHue()
         ...   def construct(self, image, delta):
         ...     return self.adjustHue(image, delta)
         ...
         >>> image = np.array([[[1, 2, 3], [4, 5, 6]],
         ...                   [[7, 8, 9], [10, 11, 12]],
         ...                   [[13, 14, 15], [16, 17, 18]]]).astype(np.float32)
         >>> delta = 0.2
         >>> adjust_hue = AdjustHue()
         >>> output = adjust_hue(Tensor(image), Tensor(delta))
         >>> print("output", output)
         output [[[ 2.3999996  1.         3.       ]
                  [ 5.3999996  4.         6.       ]]
                 [[ 8.4        7.         9.       ]
                  [11.4       10.        12.       ]]
                 [[14.4       13.        15.       ]
                  [17.4       16.        18.       ]]]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize AdjustHue"""
        self.init_prim_io_names(inputs=['images', 'delta'], outputs=['y'])


class ExtractGlimpse(Primitive):
    r"""
    Extracts glimpses(usually subarea of rectangle) from the input image Tensor and return as windows.

    Note:
        If extracted windows and the input image only partially overlap,
        random noise is filled in those non overlapping areas.

    Args:
        centered (bool, optional): An optional `bool`. Indicates if the offset coordinates
            are centered relative to the image, in which case the (0, 0) offset is relative to the center of
            the center of the input images. If ``False`` , the (0, 0) offset corresponds to the upper left corner
            of the input images. Default: ``True`` .
        normalized (bool, optional): An optional `bool`. indicates if the offset
            coordinates are normalized. Default: ``True`` .
        uniform_noise (bool, optional): An optional `bool`. indicates if the noise should be
            generated using a uniform distribution(aka. Gaussian distribution). Default: ``True`` .
        noise (str, optional): An optional string specifies the type of noise to fill.
            The window is determined by size and offsets.
            When the window and input image tensor don't not overlap, random noise is filled.
            The value can be ``'uniform'`` , ``'gaussian'`` and ``'zero'`` . Default: ``uniform`` .

            - When `noise` is ``'uniform'`` and ``'gaussian'`` , the result is variable.
            - When `noise` is ``'zero'`` , the value of `uniform_noise` must be ``'False'`` and the
              filling noise will be zero so that the result is fixed.
            - When `uniform_noise` is ``'True'`` , the value of `noise` only can be ``'uniform'`` .
              When `uniform_noise` is ``'False'`` , the value of `noise` can be ``'uniform'`` , ``'gaussian'`` or
              ``'zero'`` .

    Inputs:
        - **x** (Tensor) - A 4-D float tensor of shape :math:`(batch\_size, height, width, channels)`.
          Types allowed: float32.
        - **size** (Tensor) - A 1-D tensor of 2 elements containing the size of the glimpses to extract.
          The glimpse height must be specified first, following by the glimpse width. Types allowed: int32.
          The value of size must be greater than zero.
        - **offsets** (Tensor) - A 2-D integer tensor of shape :math:`(batch\_size, 2)` containing the y, x locations
          of the center of each window. Types allowed: float32.

    Outputs:
        A 4-D tensor of shape :math:`(batch\_size, glimpse\_height, glimpse\_width, channels)` with type: float32.

    Raises:
        TypeError: If `centered` is not a bool.
        TypeError: If `normalize` is not a bool.
        TypeError: If `uniform_noise` is not a bool.
        ValueError: If `noise` is not `uniform`, `gaussian` or `zero`.
        ValueError: If the value of `size` is not constant value.
        ValueError: If the batch_size of input is inconsistent with the batch_size of offsets.
        ValueError: If the value of offsets[1] is not 2.
        ValueError: If the input is not Tensor.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> x = Tensor([[[[0.0], [1.0], [2.0]], [[3.0], [4.0], [5.0]], [[6.0], [7.0], [8.0]]]], dtype=mindspore.float32)
        >>> size = Tensor((2, 2), dtype=mindspore.int32)
        >>> offsets = Tensor([[1, 1]], dtype=mindspore.float32)
        >>> ops = P.image_ops.ExtractGlimpse(centered = False, normalized = False,
        >>>                                  uniform_noise = False, noise = "uniform")
        >>> output = ops(x, size, offsets)
        >>> print(output)
        [[[[0.]
           [1.]]
          [[3.]
           [4.]]]]
    """
    @prim_attr_register
    def __init__(self, centered=True, normalized=True, uniform_noise=True, noise="uniform"):
        self.init_prim_io_names(inputs=['x', 'size', 'offsets'], outputs=['output'])
        self.centered = centered
        self.normalized = normalized
        self.uniform_noise = uniform_noise
        self.noise = noise
        self.add_prim_attr('centered', self.centered)
        self.add_prim_attr('normalized', self.normalized)
        self.add_prim_attr('noise', self.noise)
        self.add_prim_attr('uniform_noise', self.uniform_noise)
        validator.check_value_type("centered", centered, [bool], self.name)
        validator.check_value_type("normalized", normalized, [bool], self.name)
        validator.check_value_type("noise", noise, [str], self.name)
        validator.check_string(noise, ["uniform", "gaussian", "zero"], "noise", self.name)
        validator.check_value_type("uniform_noise", uniform_noise, [bool], self.name)
        if uniform_noise and noise != "uniform":
            raise ValueError(f"For '{self.name}', the value of 'noise' must be uniform "
                             f"when uniform_noise is True, but got {noise}.")


class CropAndResize(Primitive):
    r"""
    Extracts crops from the input image tensor and resizes them.

    Note:
        In case that the output shape depends on crop_size, the crop_size must be constant.
        For now, the backward of the operator only support bilinear method, for other methods, will return 0.

    Args:
        method (str, optional): An optional string that specifies the sampling method for resizing.
            It can be ``"bilinear"`` , ``"nearest"`` or ``"bilinear_v2"`` . Default: ``"bilinear"`` .

            - ``"nearest"``: Nearest neighbor interpolation. Each output pixel is assigned the value of the
              nearest input pixel. This method is simple and fast but can result in blocky or pixelated outputs.
            - ``"bilinear"``: Bilinear interpolation. Each output pixel is a weighted average of the four nearest input
              pixels, computed using bilinear interpolation. This method produces smoother results compared
              to nearest neighbor interpolation.
            - ``"bilinear_v2"``: The optimized variant of
              ``"bilinear"``, it may achieve better result(higher precision and speed) in some cases.

        extrapolation_value (float, optional): An optional float value used extrapolation, if applicable.
            Default: ``0.0`` .

    Inputs:
        - **x** (Tensor) - The input image must be a 4-D tensor of shape
          :math:`(batch, image\_height, image\_width, depth)`.
          Types allowed: int8, int16, int32, int64, float16, float32, float64, uint8, uint16.
        - **boxes** (Tensor) - A 2-D tensor of shape :math:`(num\_boxes, 4)`.
          The i-th row of the tensor specifies the coordinates of a box in the box_ind[i] image
          and is specified in normalized coordinates [y1, x1, y2, x2]. A normalized coordinate value of y is mapped to
          the image coordinate at y * (image_height - 1), so as the [0, 1] interval of normalized image height is
          mapped to [0, image_height - 1] in image height coordinates. We do allow y1 > y2, in which case the sampled
          crop is an up-down flipped version of the original image. The width dimension is treated similarly.
          Normalized coordinates outside the [0, 1] range are allowed, in which case we use `extrapolation_value` to
          extrapolate the input image values. Types allowed: float32.
        - **box_index** (Tensor) - A 1-D tensor of shape :math:`(num\_boxes)` with int32 values in [0, batch).
          The value of `box_index[i]` specifies the image that the i-th box refers to. Types allowed: int32.
        - **crop_size** (Tuple[int]) - A tuple of two int32 elements: (crop_height, crop_width).
          Only constant value is allowed. All cropped image patches are resized to this size.
          The aspect ratio of the image content is not preserved. Both crop_height and crop_width need to be positive.

    Outputs:
        A 4-D tensor of shape :math:`(num\_boxes, crop\_height, crop\_width, depth)` with type: float32.

    Raises:
        TypeError: If `x` or `boxes` or `box_index` is not a Tensor.
        TypeError: If `crop_size` is not a Tuple with two int32 elements.
        TypeError: If dtype of `boxes` is not float or that of `box_index` is not int.
        TypeError: If `method` is not a str.
        TypeError: If `extrapolation_value` is not a float.
        ValueError: If the shape rank of `x` is not 4.
        ValueError: If the shape rank of `boxes` is not 2.
        ValueError: If the second dim of `boxes` is not 4.
        ValueError: If the shape rank of `box_index` is not 1.
        ValueError: If the first dim of `box_index` is not equal to that of `boxes`.
        ValueError: If existing element in `box_index` is out of range `[0, batch)`.
        ValueError: If the data of `crop_size` is not positive.
        ValueError: If `method` is not one of 'bilinear', 'nearest', 'bilinear_v2'.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> import numpy as np
        >>> from mindspore import nn, ops, Tensor
        >>> class CropAndResizeNet(nn.Cell):
        ...     def __init__(self, crop_size):
        ...         super(CropAndResizeNet, self).__init__()
        ...         self.crop_and_resize = ops.CropAndResize()
        ...         self.crop_size = crop_size
        ...
        ...     def construct(self, x, boxes, box_index):
        ...         return self.crop_and_resize(x, boxes, box_index, self.crop_size)
        ...
        >>> BATCH_SIZE = 1
        >>> NUM_BOXES = 5
        >>> IMAGE_HEIGHT = 256
        >>> IMAGE_WIDTH = 256
        >>> CHANNELS = 3
        >>> image = np.random.normal(size=[BATCH_SIZE, IMAGE_HEIGHT, IMAGE_WIDTH, CHANNELS]).astype(np.float32)
        >>> boxes = np.random.uniform(size=[NUM_BOXES, 4]).astype(np.float32)
        >>> box_index = np.random.uniform(size=[NUM_BOXES], low=0, high=BATCH_SIZE).astype(np.int32)
        >>> crop_size = (24, 24)
        >>> crop_and_resize = CropAndResizeNet(crop_size=crop_size)
        >>> output = crop_and_resize(Tensor(image), Tensor(boxes), Tensor(box_index))
        >>> print(output.shape)
        (5, 24, 24, 3)
    """

    @prim_attr_register
    def __init__(self, method="bilinear", extrapolation_value=0.0):
        """Initialize CropAndResize"""
        self.init_prim_io_names(inputs=['x', 'boxes', 'box_index', 'crop_size'], outputs=['y'])
        validator.check_value_type("method", method, [str], self.name)
        validator.check_string(method, ["bilinear", "nearest", "bilinear_v2"], "method", self.name)
        self.method = method
        validator.check_value_type("extrapolation_value", extrapolation_value, [float], self.name)
        self.extrapolation_value = extrapolation_value


class NonMaxSuppressionV3(Primitive):
    r"""
    Selects a subset of bounding boxes in a greedy manner, based on their descending score.
    It removes boxes that have high intersection-over-union (IOU) overlap with previously
    selected boxes, and eliminates boxes with scores lower than a given threshold.

    .. warning::
        When input `max_output_size` is negative, it will be treated as 0.

    Note:
        - This algorithm does not depend on the location of the origin in the coordinate system.
        - This algorithm remains unaffected by orthogonal transformations and translations of
          the coordinate system, which means that translating or reflecting the coordinate system
          will result in the same boxes being chosen by the algorithm.

    Inputs:
        - **boxes** (Tensor) - A 2-D Tensor of shape :math:`(num\_boxes, 4)`.
        - **scores** (Tensor) - A 1-D Tensor of shape :math:`(num\_boxes)` where each element represents a
          single score associated with each box (i.e., each row of the `boxes` Tensor).
          It is required that the number of scores in `scores` must be equal to the number of boxes in `boxes`.
          The supported data type is float32.
        - **max_output_size** (Union[Tensor, Number.int]) - A scalar integer Tensor representing the maximum
          number of boxes to be selected by non max suppression. The supported data type is int32.
        - **iou_threshold** (Union[Tensor, Number.Float]) - A scalar float Tensor represents the threshold
          used for determining if the intersection over union (IOU) between boxes is too high.
          Data type of `iou_threshold` is float32 and must be in range [0, 1].
        - **score_threshold** (Union[Tensor, Number.Float]) - A scalar float Tensor represents the threshold for
          determining when to remove boxes based on score. The supported data type is float32.

    Outputs:
        A 1-D integer Tensor of shape :math:`(M)` representing the selected indices from the boxes tensor,
        where M <= `max_output_size`.

    Raises:
        TypeError: If the dtype of `boxes` and `scores` are different.
        TypeError: If the dtype of `iou_threshold` and `score_threshold` are different.
        TypeError: If `boxes` is not tensor or its dtype is not float16 or float32.
        TypeError: If `scores` is not tensor or its dtype is not float16 or float32.
        TypeError: If `max_output_size` is not tensor or scalar or its date type is not int32 or int64.
        TypeError: If `iou_threshold` is not tensor or scalar or its type is neither float16 or float32.
        TypeError: If `score_threshold` is not tensor or scalar or its type is neither float16 or float32.
        ValueError: If the size of shape of `boxes` is not 2 or the second value of its shape is not 4.
        ValueError: If the size of shape of `scores` is not 1.
        ValueError: If any of the size of shape of `max_output_size`,
            `iou_threshold`, `score_threshold` is not 0.

    Supported Platforms:
        ``Ascend`` ``GPU``

    Examples:
        >>> boxes = Tensor(np.array([[1, 2, 3, 4], [1, 3, 3, 4], [1, 3, 4, 4],
        ...                          [1, 1, 4, 4], [1, 1, 3, 4]]), mstype.float32)
        >>> scores = Tensor(np.array([0.4, 0.5, 0.72, 0.9, 0.45]), mstype.float32)
        >>> max_output_size = Tensor(5, mstype.int32)
        >>> iou_threshold = Tensor(0.5, mstype.float32)
        >>> score_threshold = Tensor(0, mstype.float32)
        >>> nonmaxsuppression = ops.NonMaxSuppressionV3()
        >>> output = nonmaxsuppression(boxes, scores, max_output_size, iou_threshold, score_threshold)
        >>> print(output)
        [3 2 0]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize NonMaxSuppressionV3"""
        self.init_prim_io_names(inputs=['boxes', 'scores', 'max_output_size', 'iou_threshold', 'score_threshold'],
                                outputs=['selected indices'])


class NonMaxSuppressionWithOverlaps(Primitive):
    r"""
    Selects a subset of bounding boxes in a greedy manner by prioritizing those with higher
    scores and removing those with high overlaps with previously selected boxes.
    Boxes with scores lower than the score threshold are also removed.
    The overlap values between boxes are represented as an N-by-N square matrix,
    which can be customized to define different overlap criteria such as intersection
    over union or intersection over area.


    Note:
        - This algorithm does not depend on the location of the origin in the coordinate system.
        - This algorithm remains unaffected by orthogonal transformations and translations of
          the coordinate system, which means that translating or reflecting the coordinate system
          will result in the same boxes being chosen by the algorithm.

    Inputs:
        - **overlaps** (Tensor) - A 2-D Tensor of shape :math:`(num\_boxes, num\_boxes)`,
          representing the n-by-n box overlap values. Types allowed:float16, float32 and float64.
        - **scores** (Tensor) - A 1-D Tensor of shape :math:`(num\_boxes)` where each element represents a
          single score associated with each box (i.e., each row of the `boxes` Tensor).
          It is required that the number of scores in `scores` must be equal to the number of boxes in `boxes`.
          The supported data type is float32.
        - **max_output_size** (Union[Tensor, Number.int]) - A scalar integer Tensor representing the maximum
          number of boxes to be selected by non max suppression, and max_output_size must be equal to or greater
          than 0.
          Types allowed:int32.
        - **overlap_threshold** (Union[Tensor, Number.Float]) - A scalar value, represented by a 0-D float Tensor,
          which is used as a threshold to determine if two boxes overlap too much.
          Types allowed:float16, float32 and float64.
        - **score_threshold** (Union[Tensor, Number.Float]) - A 0-D float Tensor representing the threshold for
          deciding when to remove boxes based on score. It has the same dtype as `overlap_threshold`.

    Outputs:
       A 1-D integer Tensor of shape :math:`(M)` representing the selected indices from the `boxes` Tensor,
       where M <= `max_output_size`. Its data type is int32.

    Raises:
        TypeError: If the dtype of `overlaps` , `scores` `overlap_threshold` and `score_threshold`
                   is not float16, float32 or float64.
        TypeError: If `overlaps` or `scores` is not Tensor.
        TypeError: If `max_output_size` is not Tensor or Scalar.If `max_output_size` is not int32.
        TypeError: If `overlap_threshold` is not Tensor or scalar. If its type is not float16, float32 or float64.
        TypeError: If `score_threshold` is not Tensor or scalar. If its type is not float16, float32 or float64.
        ValueError: If the size of shape of `overlaps` is not 2 or the second value of its shape
                    is not equal to the first value of its shape.
        ValueError: If the size of shape of `scores` is not 1.
        ValueError: If any of the size of shape of `max_output_size`, `overlap_threshold`, `score_threshold` is not 0.
        ValueError: If `max_output_size` is negative.
        ValueError: If the shape of `scores` is not equal to the shape of the dim0 or dim1 of `overlaps`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> overlaps = Tensor(np.array([[0.6964692, 0.28613934, 0.22685145, 0.5513148],
        ...                     [0.71946895, 0.42310646, 0.9807642, 0.6848297],
        ...                     [0.4809319, 0.39211753, 0.343178, 0.7290497],
        ...                     [0.43857226, 0.059677895, 0.39804426, 0.7379954]
        ...                     ]), mstype.float32)
        >>> scores = Tensor(np.array([0.18249173, 0.17545176, 0.53155136, 0.53182757]), mstype.float32)
        >>> max_output_size = Tensor(4, mstype.int32)
        >>> overlap_threshold = Tensor(0.1, mstype.float32)
        >>> score_threshold = Tensor(0.2, mstype.float32)
        >>> nonmaxsuppression = ops.NonMaxSuppressionWithOverlaps()
        >>> output = nonmaxsuppression(overlaps, scores, max_output_size, overlap_threshold, score_threshold)
        >>> print(output)
        [3]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize NonMaxSuppressionWithOverlaps"""
        self.init_prim_io_names(inputs=['overlaps', 'scores', 'max_output_size',
                                        'overlap_threshold', 'score_threshold'], outputs=['selected_indices'])


class HSVToRGB(Primitive):
    r"""
    Transform one single or a batch of images from HSV to RGB color space.
    Each pixel's HSV value is converted to its corresponding RGB value.
    Note that the function is only well-defined for input pixel values in the range [0, 1].
    Image format should be "NHWC".

    Inputs:
        - **x** (Tensor) - The input image must be a 4-D tensor of shape
          :math:`(batch, image\_height, image\_width, channel)`.
          Number of channel must be 3. Types allowed: float16, float32, float64.

    Outputs:
        A 4-D tensor of shape :math:`(batch, image\_height, image\_width, channel)`
        with same type of input.

    Raises:
        TypeError: If `x` is not a Tensor.
        TypeError: If the dtype of `x` is not float16, float32, float64.
        ValueError: If rank of the `x` is not equal to 4.
        ValueError: If the last dimension of `x` is not equal to 3.

    Supported Platforms:
        ``GPU`` ``CPU``

    Examples:
        >>> image = np.array([0.5, 0.5, 0.5]).astype(np.float32).reshape([1, 1, 1, 3])
        >>> hsv_to_rgb = ops.HSVToRGB()
        >>> output = hsv_to_rgb(Tensor(image))
        >>> print(output)
        [[[[0.25 0.5  0.5 ]]]]
    """
    @prim_attr_register
    def __init__(self):
        pass


class CropAndResizeGradBoxes(Primitive):
    """
    Computes the gradient of the CropAndResize op with respect to the input boxes tensor.

    Note:
        Input images and grads must be a 4-D tensor.

    Args:
        method (str): A string specifying the interpolation method. Only ``"bilinear"`` is supported for now.
            Default: ``"bilinear"`` .

    Inputs:
        - **grads** (Tensor) - A 4-D tensor of shape [num_boxes, crop_height, crop_width, depth].
          The format must be NHWC. Types allowed: float32, float64.
        - **images** (Tensor) - A 4-D tensor of shape [batch, image_height, image_width, depth].
          The format must be NHWC. Types allowed: int8, int16, int32, int64, float16, float32, float64, uint8, uint16.
          Both image_height and image_width need to be positive.
        - **boxes** (Tensor) - A 2-D tensor of shape [num_boxes, 4].
          The i-th row of the tensor specifies the coordinates of a box in the box_index[i] image
          and is specified in normalized coordinates [y1, x1, y2, x2]. A normalized coordinate value of y is mapped to
          the image coordinate at y * (image_height - 1), so as the [0, 1] interval of normalized image height is
          mapped to [0, image_height - 1] in image height coordinates. We do allow y1 > y2, in which case the sampled
          crop is an up-down flipped version of the original image. The width dimension is treated similarly.
          Normalized coordinates outside the [0, 1] range are allowed, in which case we use extrapolation_value to
          extrapolate the input image values. Types allowed: float32, float64.
        - **box_index** (Tensor) - A 1-D tensor of shape [num_boxes] with int32 values in [0, batch).
          The value of box_index[i] specifies the image that the i-th box refers to. Types allowed: int32.

    Outputs:
        A 2-D tensor of shape [num_boxes, 4] with type: float32 or float64.

    Raises:
        TypeError: If `method` is not a str.
        TypeError: If `grads` is not tensor or its dtype is not float32 or float64.
        TypeError: If `images` is not tensor or its dtype is incorrect.
        TypeError: If `boxes` is not tensor or its dtype is not float32 or float64.
        TypeError: If `box_index` is not tensor or its dtype is not int32.
        ValueError: If `method` is not 'bilinear'.
        ValueError: If the size of `grads` tensor shape is not equal to 4.
        ValueError: If the size of `images` tensor shape is not equal to 4.
        ValueError: If the value of image_height or image_width of `image` tensor shape is not positive.
        ValueError: If the size of `boxes` tensor shape is not equal to 2.
        ValueError: If the length of the second dimension of `boxes` is not equal to 4.
        ValueError: If the size of `box_index` tensor shape is not equal to 1.
        ValueError: If the length of `box_index` is not equal to num_boxes.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> crop_and_resize_grad_boxes = ops.CropAndResizeGradBoxes(method = "bilinear")
        >>> grads = Tensor(np.array([[[[2.0], [5.0]], [[1.0], [4.0]]]]), mindspore.float32)
        >>> image = Tensor(np.array([[[[9.0], [5.0], [2.0], [1.0]],
        ...                           [[6.0], [1.0], [9.0], [7.0]],
        ...                           [[6.0], [0.0], [2.0], [9.0]],
        ...                           [[1.0], [2.0], [6.0], [7.0]]]]), mindspore.float32)
        >>> boxes = Tensor(np.array([[0.3, 0.8, 0.3, 0.8]]), mindspore.float32)
        >>> box_index = Tensor(np.array([0]), mindspore.int32)
        >>> output = crop_and_resize_grad_boxes(grads, image, boxes, box_index)
        >>> print(output.asnumpy())
        [138.6,-17.1,99.0,-51.300003]
    """

    @prim_attr_register
    def __init__(self, method="bilinear"):
        """Initialize CropAndResizeGradBoxes"""
        self.init_prim_io_names(inputs=['grads', 'images', 'boxes', 'box_index'], outputs=['y'])
        validator.check_value_type("method", method, [str], self.name)
        validator.check_string(method, ["bilinear"], "method", self.name)
        self.method = method


class RGBToHSV(Primitive):
    """
    Transform one single or a batch of images from RGB to HSV color space.
    Each pixel's RGB value is converted to its corresponding HSV value.
    Note that the function is only well-defined for input pixel values in the range [0, 1].

    Note:
        Last dimension of input images must be size 3.

    Inputs:
        - **images** (Tensor) - 1-D or higher rank RGB data Tensor to convert, last dimension must be size 3.
          Must be one of the following types: float16, float32, float64.

    Outputs:
        A Tensor, has the same type and shape as input `images`.

    Raises:
        TypeError: If `images` is not tensor or its dtype is not float.
        ValueError: If the rank of `images` is less than 1.
        ValueError: If the last value of shape of `images` is not 3.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> images =  np.array([0.25, 0.5, 0.5]).astype(np.float32).reshape([1, 1, 1, 3])
        >>> rgb_to_hsv = ops.RGBToHSV()
        >>> output = rgb_to_hsv(Tensor(images))
        >>> print(output)
        [[[[0.5, 0.5, 0.5]]]]
    """

    @prim_attr_register
    def __init__(self):
        """Initialize RGBToHSV"""
        self.init_prim_io_names(inputs=['images'], outputs=['y'])


class ResizeArea(Primitive):
    r"""
    Resize images to a certain size using area interpolation.

    The resizing process only changes the two dimensions of images, which represent the width and height of images.

    .. warning::
        The values of `size` must be greater than zero.

    Args:
        align_corners (bool, optional): A boolean flag that specifies whether
            to align the centers of the four corner pixels of the input and output tensors.
            When this flag is set to ``True`` , the corner pixels of the output tensor are aligned
            with the corner pixels of the input tensor, which preserves the values at the corner pixels.
            Default: ``False`` .

    Inputs:
        - **images** (Tensor) -  Input images must be a 4-D tensor with shape
          which is :math:`(batch, channels, height, width)`. The format must be "NHWC".
        - **size** (Tensor) - Input size must be a 1-D tensor of 2 elements: new_height, new_width.
          The new size of output image.
          Types allowed: int32.

    Outputs:
        A 4-D tensor of shape :math:`(batch, new\_height, new\_width, channels)` with type float32.

    Raises:
        TypeError: If dtype of `images` is not supported.
        TypeError: If dtype of `size` is not int32.
        TypeError: If dtype of `align_corners` is not bool.
        ValueError: If the num of inputs is not 2.
        ValueError: If the dimension of `images` is not 4.
        ValueError: If the dimension of `size` is not 1.
        ValueError: If the element num of `size` is not 2.
        ValueError: If any value of `size` is not positive.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> images = Tensor([[[[2], [4], [6], [8]], [[10], [12], [14], [16]]]], mindspore.float16)
        >>> size = Tensor([1, 2], mindspore.int32)
        >>> resizearea = ops.ResizeArea()
        >>> output = resizearea(images, size)
        >>> print(output.asnumpy())
            [[[[ 7.]
               [11.]]]]
    """

    @prim_attr_register
    def __init__(self, align_corners=False):
        """Initialize ResizeArea"""
        self.init_prim_io_names(inputs=['images', 'size'], outputs=['y'])
        validator.check_value_type("align_corners", align_corners, [bool], self.name)
        self.align_corners = align_corners


class CropAndResizeGradImage(Primitive):
    """
    Computes the gradient of the CropAndResize op with respect to the input images tensor.

    Note:
        Input grads must be a 4-D tensor.

    Args:
        method (str): A string specifying the interpolation method. ``"bilinear"`` , ``"nearest"`` and
            ``"bilinear_v2"`` are supported for now. ``"bilinear_v2"`` only supports GPU. Default: ``"bilinear"`` .
        T (mindspore.dtype): T is a required attribute. The value range of T is {mindspore.float16, mindspore.float32,
            mindspore.float64}.

    Inputs:
        - **grads** (Tensor) - A 4-D tensor of shape [num_boxes, crop_height, crop_width, depth].
          The format must be NHWC. Types allowed: float32, float64.
        - **boxes** (Tensor) - A 2-D tensor of shape [num_boxes, 4].
          The i-th row of the tensor specifies the coordinates of a box in the box_index[i] image
          and is specified in normalized coordinates [y1, x1, y2, x2]. A normalized coordinate value of y is mapped to
          the image coordinate at y * (image_height - 1), so as the [0, 1] interval of normalized image height is
          mapped to [0, image_height - 1] in image height coordinates. We do allow y1 > y2, in which case the sampled
          crop is an up-down flipped version of the original image. The width dimension is treated similarly.
          Normalized coordinates outside the [0, 1] range are allowed, in which case we use extrapolation_value to
          extrapolate the input image values. Types allowed: float32, float64.
        - **box_index** (Tensor) - A 1-D tensor of shape [num_boxes] with int32 values in [0, batch).
          The value of box_index[i] specifies the image that the i-th box refers to. Types allowed: int32.
        - **image_size** (Tensor) - A 1-D tensor with value [batch, image_height, image_width, depth]
          containing the original image size. Both image_height and image_width need to be positive.
          Types allowed: int32.

    Outputs:
        A 4-D tensor of shape [batch, image_height, image_width, depth]. Output type depends on input attribute T.
        Types allowed: mindspore.float16, mindspore.float32, mindspore.float64.

    Raises:
        TypeError: If `method` is not a str.
        TypeError: If `grads` is not tensor or its dtype is not float32 or float64.
        TypeError: If `boxes` is not tensor or its dtype is not float32 or float64.
        TypeError: If `box_index` is not tensor or its dtype is not int32.
        TypeError: If `image_size` is not tensor or its dtype is not int32.
        TypeError: If the value of `T` is not a number dtype in mindspore.
        ValueError: If `method` is not in {"bilinear", "nearest", "bilinear_v2"}.
        ValueError: If `T` is not in {mindspore.float16, mindspore.float32, mindspore.float64}.
        ValueError: If the size of `grads` tensor shape is not equal to 4.
        ValueError: If the size of `boxes` tensor shape is not equal to 2.
        ValueError: If the length of the second dimension of `boxes` is not equal to 4.
        ValueError: If the size of `image_size` or `box_index` tensor shape is not equal to 1.
        ValueError: If the length of `box_index` is not equal to num_boxes.
        ValueError: If the length of `image_size` is not equal to 4.
        ValueError: If the value of image_height or image_width of `image_size` is not positive.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> crop_and_resize_grad_image = ops.CropAndResizeGradImage(T = mindspore.float32, method = "bilinear")
        >>> grads = Tensor(np.array([[[[1.0], [2.0]], [[3.0], [4.0]]]]), mindspore.float32)
        >>> boxes = Tensor(np.array([[0.1, 0.2, 0.3, 0.4]]), mindspore.float32)
        >>> box_index = Tensor(np.array([0]), mindspore.int32)
        >>> image_size = Tensor(np.array([1, 4, 4, 1]), mindspore.int32)
        >>> output = crop_and_resize_grad_image(grads, boxes, box_index, image_size)
        >>> print(output.asnumpy())
        [[[[0.39999992]
           [2.0399997 ]
           [0.36000004]
           [0.        ]]
          [[1.1999999 ]
           [5.16      ]
           [0.8400003 ]
           [0.        ]]
          [[0.        ]
           [0.        ]
           [0.        ]
           [0.        ]]
          [[0.        ]
           [0.        ]
           [0.        ]
           [0.        ]]]]
    """

    @prim_attr_register
    def __init__(self, T, method="bilinear"):
        """Initialize CropAndResizeGradImage"""
        self.init_prim_io_names(inputs=['grads', 'boxes', 'box_index', 'image_size'], outputs=['y'])
        validator.check_value_type("method", method, [str], self.name)
        is_ascend_cpu = context.get_context('device_target') in ("Ascend", "CPU")
        if is_ascend_cpu:
            validator.check("method", method, "expected", ("bilinear", "nearest"), validator.IN, self.name)
        else:
            validator.check("method", method, "expected", ("bilinear", "nearest", "bilinear_v2"),
                            validator.IN, self.name)
        self.method = method
        valid_values = (mstype.float16, mstype.float32, mstype.float64)
        if T in mstype.number_type:
            validator.check("T", T, "expected", valid_values, validator.IN, self.name)
        else:
            validator.check_type_name("T", T, valid_values, self.name)
        self.add_prim_attr("max_Byte", int(2e9))  # Maximum bytes of image gradient


class ScaleAndTranslate(Primitive):
    r"""
    Scale And Translate the input image tensor.

    Note:
        - Input images must be a 4-D tensor.
        - Input size, scale and translation must be a 1-D tensor with two elements.

    Args:
        kernel_type (str, optional): Deciding which image filtering algorithm to choose. Valid options:
            [ ``"lanczos1"`` , ``"lanczos3"`` , ``"lanczos5"`` , ``"gaussian"`` , ``"box"`` , ``"triangle"`` ,
            ``"keyscubic"`` , ``"mitchellcubic"`` ]. Default: ``"lanczos3"`` .
        antialias (bool, optional): Deciding whether to use the antialias. Default: ``True`` .

    Inputs:
        - **images** (Tensor) - A 4-D tensor of shape :math:`(batch, image\_height, image\_width, channel)`.
        - **size** (Tensor) - The size of the output image after scale and translate operations. A 1-D tensor with two
          positive elements whose dtype is int32 and shape must be :math:`(2,)`.
        - **scale** (Tensor) - Indicates the zoom factor. A 1-D tensor with two positive elements whose dtype is float32
          and shape must be :math:`(2,)`.
        - **translation** (Tensor) - Translate the pixel value. A 1-D tensor with two elements whose dtype is
          float32 and shape must be :math:`(2,)`.

    Outputs:
        A 4-D tensor with type: float32 and shape :math:`(batch, size[0], size[1], channel)`.

    Raises:
        TypeError: If `kernel_type` is not str.
        TypeError: If `antialias` is not bool.
        TypeError: If `images` is not tensor with valid dtype.
        TypeError: If `size` is not a tensor of int32.
        TypeError: If `scale` is not a tensor of float32.
        TypeError: If `translation` is not a tensor of float32.
        ValueError: If `kernel_type` is not in ["lanczos1", "lanczos3", "lanczos5", "gaussian", "box", "triangle",
                    "keyscubic", "mitchellcubic"].
        ValueError: If the rank of `images` is not 4.
        ValueError: If the shape of `size` is not :math:`(2,)`.
        ValueError: If the shape of `scale` is not :math:`(2,)`.
        ValueError: If the shape of `translation`  is not :math:`(2,)`.

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> op = ops.ScaleAndTranslate()
        >>> image = Tensor(np.array([[[[9.0], [5.0], [2.0], [1.0]],
        ...                           [[6.0], [1.0], [9.0], [7.0]]]]), mindspore.float32)
        >>> size = Tensor(np.array([2, 2]).astype(np.int32))
        >>> scale = Tensor(np.array([1, 1]).astype(np.float32))
        >>> translation = Tensor(np.array([1, 1]).astype(np.float32))
        >>> output = op(image, size, scale, translation)
        >>> print(output)
        [[[[0.]
           [0.]]
          [[0.]
           [9.]]]]
    """

    @prim_attr_register
    def __init__(self, kernel_type="lanczos3", antialias=True):
        """Initialize ScaleAndTranslate"""
        validator.check_value_type("kernel_type", kernel_type, [str], self.name)
        validator.check_string(kernel_type, ["lanczos1", "lanczos3", "lanczos5", "gaussian", "box", "triangle",
                                             "keyscubic", "mitchellcubic"], "kernel_type", self.name)
        validator.check_value_type("antialias", antialias, [bool], self.name)


class CombinedNonMaxSuppression(Primitive):
    r"""
    Applies a greedy approach to select a subset of bounding boxes from a list of
    candidates using NonMaxSuppression, where the boxes are sorted in descending order of their confidence score.

    Args:
        clip_boxes (bool, optional): Determines whether to apply bounding box normalization to ensure the
            coordinates are within [0, 1] range. Default: ``True`` .

            - If ``True`` , clip the boxes that fall outside this range.
            - If ``False`` , return the box coordinates as they are without any modifications.

        pad_per_class (bool, optional): Determines whether the output of the non-maximum suppression (NMS)
            algorithm should be padded or clipped to meet the maximum size constraints. Default: ``False`` .

            - If ``False`` , the output is clipped to the maximum size of `max_total_size`.
            - If ``True`` , the output is padded up to `max_size_per_class` * `num_classes` and clipped if
              it exceeds `max_total_size`.

    Inputs:
        - **boxes** (Tensor) - A float32 Tensor with shape :math:`(batch\_size, num\_boxes, q, 4)`
          representing the bounding box coordinates.
          `q` indicates mapping relationship between boxes and classes.
          If `q` is 1, all classes use the same bounding box. If `q` is equal to the number of classes,
          class-specific boxes are applied.
        - **scores** (Tensor) - A 3-D Tensor of float32 type with the shape
          :math:`(batch\_size, num\_boxes, num\_classes)`. It contains a score value for each box,
          with each row of `boxes` represented by a single score.
        - **max_output_size_per_class** (Tensor) - The maximum number of boxes that can be selected for each class
          by the non-maximum suppression algorithm, represented by a scalar Tensor of type int32.
        - **max_total_size** (Tensor) - A scalar Tensor of type int32 that represents the
          maximum number of boxes that are kept for all classes.
        - **iou_threshold** (Tensor) - A scalar Tensor of float32 type that represents the threshold for
          determining if the IOU overlap between boxes is too high. `iou_threshold` must be equal or greater
          than 0 and be equal or smaller than 1.
        - **score_threshold** (Tensor) - A scalar Tensor of type float32 that represents the threshold
          for determining when to remove boxes based on their scores.

    Outputs:
        - **nmsed_boxes** - A Tensor of float32 with shape of (batch_size, num_detection, 4), which contains
          the non-max suppressed boxes.
        - **nmsed_scores** - A Tensor of float32 with shape of (batch_size, num_detection), which contains score
          of boxes.
        - **nmsed_classes** - A Tensor of float32 with shape of (batch_size, num_detection), which contains classes
          of boxes.
        - **valid_detections** A Tensor of int32 with shape of (batch_size,), which indicates the number of valid
          detections of each batch.

    Raises:
        TypeError: If the dtype of `boxes`, `scores` , `iou_threshold` , `score threshold` are not float32.
        TypeError: If the dtype of `max_output_size_per_class` and `max_total_size` are not int32.
        ValueError: If `boxes` is not 4D.
        ValueError: If `max_output_size_per_class`, `max_total_size`, `iou_threshold` and `score threshold` are not 0D.
        ValueError: If `scores` is not 3D.
        ValueError: If shape[0] or shape[1] of `boxes` is not same with that of the `scores`.
        ValueError: If shape[2] of `boxes` is not same with shape[2] of `scores` or 1
        ValueError: If `max_total_size` < 0.
        ValueError: If `max_output_size_per_class` < 0.
        ValueError: If `iou_threshold` not in [0,1].

    Supported Platforms:
        ``Ascend`` ``GPU`` ``CPU``

    Examples:
        >>> boxes = Tensor(np.array([[[[200, 100, 150, 100]],
        ...                           [[220, 120, 150, 100]],
        ...                           [[190, 110, 150, 100]],
        ...                           [[210, 112, 150, 100]]]])).astype('float32')
        >>> scores = Tensor(np.array([[[0.2000, 0.7000, 0.1000], [0.1000, 0.8000, 0.1000], [0.3000, 0.6000, 0.1000],
        ...                            [0.0500, 0.9000, 0.0500]]])).astype('float32')
        >>> max_output_size_per_class = Tensor(4, mstype.int32)
        >>> max_total_size = Tensor(1, mstype.int32)
        >>> iou_threshold = Tensor(0, mstype.float32)
        >>> score_threshold = Tensor(0, mstype.float32)
        >>> net = ops.CombinedNonMaxSuppression()
        >>> out = net(boxes, scores, max_output_size_per_class, max_total_size, iou_threshold, score_threshold)
        >>> print(out)
        (Tensor(shape=[1, 1, 4], dtype=Float32, value= [[[1.00000000e+00, 1.00000000e+00, 1.00000000e+00,
                                                          1.00000000e+00]]]),
        Tensor(shape=[1, 1], dtype=Float32, value= [[ 8.99999976e-01]]),
        Tensor(shape=[1, 1], dtype=Float32, value= [[ 1.00000000e+00]]),
        Tensor(shape=[1], dtype=Int32, value= [1]))
    """

    @prim_attr_register
    def __init__(self, pad_per_class=False, clip_boxes=True):
        """Initialize CombinedNonMaxSuppression"""
        self.pad_per_class = validator.check_value_type("pad_per_class", pad_per_class, [bool], self.name)
        self.add_prim_attr('pad_per_class', self.pad_per_class)
        self.clip_boxes = validator.check_value_type("clip_boxes", clip_boxes, [bool], self.name)
        self.add_prim_attr('clip_boxes', self.clip_boxes)


class ResizeV2(Primitive):
    r"""
    Using the nearest, linear or cubic interpolate method resize the input tensor 'x'.

    Note:
        Input x must be a 4-D tensor.

    Args:
        coordinate_transformation_mode (str): Default is ``'half_pixel'`` . Describes how to transform the
            coordinate in the resized tensor to the coordinate in the original tensor. Other optional:
            ``'align_corners'`` . In ``'nearest'`` mode, coordinate_transformation_mode must be ``'half_pixel'`` .
        mode (str): Default: ``'nearest'`` . Other optional: ``'linear'`` and ``'cubic'`` .

    Inputs:
        - **x** (Tensor) - A 4-D tensor which to resize, with shape [batch, channel, width, height]. Must be one of the
          following types: uint8, int8, int16, int32, int64, float16, float32, float64, when mode = 'nearest'.
          Must be one of the following types: float16, float32, float64, when mode = 'linear' or 'cubic'.
        - **roi** (Tensor) - A 1-D float32 Tensor. Unused parameters currently.
        - **scales** (Tensor) - A 1-D float32 Tensor. Unused parameters currently.
        - **sizes** (Tensor) - A 1-D int64 or int32 Tensor, the length must be 4 and greater than 0.
          And sizes[0], sizes[1] must match with the shape[0] and shape[1] of x.
          When mode equals 'nearest' or 'linear', sizes[2] must be 1.

    Outputs:
        A 4-D tensor which shape is [batch, channel, new_height, new_width] with type as same as x.

    Raises:
        TypeError: If dtype of `x`, `roi`, `scales` or `sizes` is not supported.
        ValueError: If shape of `x`, `roi`, `scales` or `sizes` is not supported.
        ValueError: If the length of `sizes` is not 4.
        ValueError: If `sizes` is not greater than 0.
        ValueError: If sizes[2] is not 1, when `mode` = 'nearest' or 'linear'.
        ValueError: If sizes[0] and sizes[1] don't match the shape[0] and shape[1] of x.
        ValueError: If `coordinate_transformation_mode` or `mode` is not supported.
        ValueError: If `coordinate_transformation_mode` is not 'half_pixel', when `mode` = 'nearest'.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> x = Tensor(np.array([[[[1., 2., 3., 4.]]]]).astype(np.float32))
        >>> roi = Tensor(np.array([0]).astype(np.float32))
        >>> scales = Tensor(np.array([0]).astype(np.float32))
        >>> sizes = Tensor(np.array([1, 1, 1, 9]).astype(np.int64))
        >>> resize_v2 = ops.ResizeV2(coordinate_transformation_mode="half_pixel", mode="nearest")
        >>> output = resize_v2(x, roi, scales, sizes)
        >>> print(output)
        [[[[1. 1. 1. 2. 2. 3. 3. 4. 4.]]]]
    """
    @prim_attr_register
    def __init__(self, coordinate_transformation_mode="half_pixel", mode="nearest"):
        """Initialize ResizeV2."""
        self.init_prim_io_names(inputs=['x', 'roi', 'scales', 'sizes'], outputs=['y'])
        self.add_prim_attr("nearest_mode", "floor")
        self.add_prim_attr("cubic_coeff_a", -0.75)
        validator.check_value_type(
            "coordinate_transformation_mode", coordinate_transformation_mode, [str], self.name)
        validator.check_string(coordinate_transformation_mode, ["align_corners", "half_pixel"],
                               "coordinate_transformation_mode", self.name)
        validator.check_value_type("mode", mode, [str], self.name)
        validator.check_string(mode, ["nearest", "linear", "cubic"], "mode", self.name)
