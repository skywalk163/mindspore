# Copyright 2021-2022 Huawei Technologies Co., Ltd
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
# ==============================================================================
"""
The module audio.transforms is inherited from _c_dataengine and is
implemented based on C++. It's a high performance module to process
audio. Users can apply suitable augmentations on audio data to improve
their training models.
"""

import numpy as np

import mindspore._c_dataengine as cde
from .utils import BorderType, DensityFunction, FadeShape, GainType, Interpolation, MelType, Modulation, NormMode, \
    NormType, ResampleMethod, ScaleType, WindowType
from .validators import check_allpass_biquad, check_amplitude_to_db, check_band_biquad, check_bandpass_biquad, \
    check_bandreject_biquad, check_bass_biquad, check_biquad, check_complex_norm, check_compute_deltas, \
    check_contrast, check_db_to_amplitude, check_dc_shift, check_deemph_biquad, check_detect_pitch_frequency, \
    check_dither, check_equalizer_biquad, check_fade, check_flanger, check_gain, check_griffin_lim, \
    check_highpass_biquad, check_inverse_mel_scale, check_inverse_spectrogram, check_lfcc, check_lfilter, \
    check_lowpass_biquad, check_magphase, check_mask_along_axis, check_mask_along_axis_iid, check_masking, \
    check_mel_scale, check_mel_spectrogram, check_mfcc, check_mu_law_coding, check_overdrive, check_phase_vocoder, \
    check_phaser, check_pitch_shift, check_resample, check_riaa_biquad, check_sliding_window_cmn, \
    check_spectral_centroid, check_spectrogram, check_time_stretch, check_treble_biquad, check_vad, check_vol
from ..transforms.py_transforms_util import Implementation
from ..transforms.transforms import TensorOperation


class AudioTensorOperation(TensorOperation):
    """
    Base class of Audio Tensor Ops.
    """

    def __init__(self):
        super().__init__()
        self.implementation = Implementation.C

    def __call__(self, *input_tensor_list):
        for tensor in input_tensor_list:
            if not isinstance(tensor, (np.ndarray,)):
                raise TypeError("Input should be NumPy audio, got {}.".format(type(tensor)))
        return super().__call__(*input_tensor_list)

    def parse(self):
        raise NotImplementedError("AudioTensorOperation has to implement parse() method.")


class AllpassBiquad(AudioTensorOperation):
    r"""
    Design two-pole all-pass filter with central frequency and bandwidth for audio waveform.

    An all-pass filter changes the audio's frequency to phase relationship without changing
    its frequency to amplitude relationship. The system function is:

    .. math::
        H(s) = \frac{s^2 - \frac{s}{Q} + 1}{s^2 + \frac{s}{Q} + 1}

    Similar to `SoX <https://sourceforge.net/projects/sox/>`_ implementation.

    Note:
        The shape of the audio waveform to be processed needs to be <..., time>.

    Args:
        sample_rate (int): Sampling rate (in Hz), which can't be zero.
        central_freq (float): Central frequency (in Hz).
        Q (float, optional): `Quality factor <https://en.wikipedia.org/wiki/Q_factor>`_ ,
            in range of (0, 1]. Default: ``0.707``.

    Raises:
        TypeError: If `sample_rate` is not of type int.
        ValueError: If `sample_rate` is 0.
        TypeError: If `central_freq` is not of type float.
        TypeError: If `Q` is not of type float.
        ValueError: If `Q` is not in range of (0, 1].
        RuntimeError: If input tensor is not in shape of <..., time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode.
        >>> waveform = np.random.random([5, 16])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.AllpassBiquad(44100, 200.0)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (16,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([16])  # 1 sample
        >>> output = audio.AllpassBiquad(44100, 200.0)(waveform)
        >>> print(output.shape, output.dtype)
        (16,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_allpass_biquad
    def __init__(self, sample_rate, central_freq, Q=0.707):
        super().__init__()
        self.sample_rate = sample_rate
        self.central_freq = central_freq
        self.quality_factor = Q

    def parse(self):
        return cde.AllpassBiquadOperation(self.sample_rate, self.central_freq, self.quality_factor)


DE_C_SCALE_TYPE = {ScaleType.POWER: cde.ScaleType.DE_SCALE_TYPE_POWER,
                   ScaleType.MAGNITUDE: cde.ScaleType.DE_SCALE_TYPE_MAGNITUDE}


class AmplitudeToDB(AudioTensorOperation):
    r"""
    Turn the input audio waveform from the amplitude/power scale to decibel scale.

    Note:
        The shape of the audio waveform to be processed needs to be <..., freq, time>.

    Args:
        stype (ScaleType, optional): Scale of the input waveform, which can be
            ``ScaleType.POWER`` or ``ScaleType.MAGNITUDE``. Default: ``ScaleType.POWER``.
        ref_value (float, optional): Multiplier reference value for generating
            `db_multiplier` . Default: ``1.0``. The formula is

            :math:`\text{db_multiplier} = \log10(\max(\text{ref_value}, amin))` .

        amin (float, optional): Lower bound to clamp the input waveform, which must
            be greater than zero. Default: ``1e-10``.
        top_db (float, optional): Minimum cut-off decibels, which must be non-negative. Default: ``80.0``.

    Raises:
        TypeError: If `stype` is not of type :class:`mindspore.dataset.audio.ScaleType` .
        TypeError: If `ref_value` is not of type float.
        ValueError: If `ref_value` is not a positive number.
        TypeError: If `amin` is not of type float.
        ValueError: If `amin` is not a positive number.
        TypeError: If `top_db` is not of type float.
        ValueError: If `top_db` is not a positive number.
        RuntimeError: If input tensor is not in shape of <..., freq, time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 400 // 2 + 1, 30])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.AmplitudeToDB(stype=audio.ScaleType.POWER)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (201, 30) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([400 // 2 + 1, 30])  # 1 sample
        >>> output = audio.AmplitudeToDB(stype=audio.ScaleType.POWER)(waveform)
        >>> print(output.shape, output.dtype)
        (201, 30) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_amplitude_to_db
    def __init__(self, stype=ScaleType.POWER, ref_value=1.0, amin=1e-10, top_db=80.0):
        super().__init__()
        self.stype = stype
        self.ref_value = ref_value
        self.amin = amin
        self.top_db = top_db

    def parse(self):
        return cde.AmplitudeToDBOperation(DE_C_SCALE_TYPE.get(self.stype), self.ref_value, self.amin, self.top_db)


class Angle(AudioTensorOperation):
    """
    Calculate the angle of complex number sequence.

    Note:
        The shape of the audio waveform to be processed needs to be <..., complex=2>.
        The first dimension represents the real part while the second represents the imaginary.

    Raises:
        RuntimeError: If input tensor is not in shape of <..., complex=2>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 16, 2])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.Angle()]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (16,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([16, 2])  # 1 sample
        >>> output = audio.Angle()(waveform)
        >>> print(output.shape, output.dtype)
        (16,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    def parse(self):
        return cde.AngleOperation()


class BandBiquad(AudioTensorOperation):
    """
    Design two-pole band-pass filter for audio waveform.

    The frequency response drops logarithmically around the center frequency. The
    bandwidth gives the slope of the drop. The frequencies at band edge will be
    half of their original amplitudes.

    Similar to `SoX <https://sourceforge.net/projects/sox/>`_ implementation.

    Note:
        The shape of the audio waveform to be processed needs to be <..., time>.

    Args:
        sample_rate (int): Sampling rate (in Hz), which can't be zero.
        central_freq (float): Central frequency (in Hz).
        Q (float, optional): `Quality factor <https://en.wikipedia.org/wiki/Q_factor>`_ ,
            in range of (0, 1]. Default: ``0.707``.
        noise (bool, optional) : If ``True``, uses the alternate mode for un-pitched audio (e.g. percussion).
            If ``False``, uses mode oriented to pitched audio, i.e. voice, singing, or instrumental music.
            Default: ``False``.

    Raises:
        TypeError: If `sample_rate` is not of type int.
        ValueError: If `sample_rate` is 0.
        TypeError: If `central_freq` is not of type float.
        TypeError: If `Q` is not of type float.
        ValueError: If `Q` is not in range of (0, 1].
        TypeError: If `noise` is not of type bool.
        RuntimeError: If input tensor is not in shape of <..., time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 16])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.BandBiquad(44100, 200.0)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (16,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([16])  # 1 sample
        >>> output = audio.BandBiquad(44100, 200.0)(waveform)
        >>> print(output.shape, output.dtype)
        (16,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_band_biquad
    def __init__(self, sample_rate, central_freq, Q=0.707, noise=False):
        super().__init__()
        self.sample_rate = sample_rate
        self.central_freq = central_freq
        self.quality_factor = Q
        self.noise = noise

    def parse(self):
        return cde.BandBiquadOperation(self.sample_rate, self.central_freq, self.quality_factor, self.noise)


class BandpassBiquad(AudioTensorOperation):
    r"""
    Design two-pole Butterworth band-pass filter for audio waveform.

    The frequency response of the Butterworth filter is maximally flat (i.e. has no ripples)
    in the passband and rolls off towards zero in the stopband.

    The system function of Butterworth band-pass filter is:

    .. math::
        H(s) = \begin{cases}
            \frac{s}{s^2 + \frac{s}{Q} + 1}, &\text{if const_skirt_gain=True}; \cr
            \frac{\frac{s}{Q}}{s^2 + \frac{s}{Q} + 1}, &\text{if const_skirt_gain=False}.
        \end{cases}

    Similar to `SoX <https://sourceforge.net/projects/sox/>`_ implementation.

    Note:
        The shape of the audio waveform to be processed needs to be <..., time>.

    Args:
        sample_rate (int): Sampling rate (in Hz), which can't be zero.
        central_freq (float): Central frequency (in Hz).
        Q (float, optional): `Quality factor <https://en.wikipedia.org/wiki/Q_factor>`_ ,
            in range of (0, 1]. Default: ``0.707``.
        const_skirt_gain (bool, optional) : If ``True``, uses a constant skirt gain (peak gain = Q);
            If ``False``, uses a constant 0dB peak gain. Default: ``False``.

    Raises:
        TypeError: If `sample_rate` is not of type int.
        ValueError: If `sample_rate` is 0.
        TypeError: If `central_freq` is not of type float.
        TypeError: If `Q` is not of type float.
        ValueError: If `Q` is not in range of (0, 1].
        TypeError: If `const_skirt_gain` is not of type bool.
        RuntimeError: If input tensor is not in shape of <..., time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 16])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.BandpassBiquad(44100, 200.0)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (16,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([16])  # 1 sample
        >>> output = audio.BandpassBiquad(44100, 200.0)(waveform)
        >>> print(output.shape, output.dtype)
        (16,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_bandpass_biquad
    def __init__(self, sample_rate, central_freq, Q=0.707, const_skirt_gain=False):
        super().__init__()
        self.sample_rate = sample_rate
        self.central_freq = central_freq
        self.quality_factor = Q
        self.const_skirt_gain = const_skirt_gain

    def parse(self):
        return cde.BandpassBiquadOperation(self.sample_rate, self.central_freq, self.quality_factor,
                                           self.const_skirt_gain)


class BandrejectBiquad(AudioTensorOperation):
    r"""
    Design two-pole Butterworth band-reject filter for audio waveform.

    The frequency response of the Butterworth filter is maximally flat (i.e. has no ripples)
    in the passband and rolls off towards zero in the stopband.

    The system function of Butterworth band-reject filter is:

    .. math::
        H(s) = \frac{s^2 + 1}{s^2 + \frac{s}{Q} + 1}

    Similar to `SoX <https://sourceforge.net/projects/sox/>`_ implementation.

    Note:
        The shape of the audio waveform to be processed needs to be <..., time>.

    Args:
        sample_rate (int): Sampling rate (in Hz), which can't be zero.
        central_freq (float): Central frequency (in Hz).
        Q (float, optional): `Quality factor <https://en.wikipedia.org/wiki/Q_factor>`_ ,
            in range of (0, 1]. Default: ``0.707``.

    Raises:
        TypeError: If `sample_rate` is not of type int.
        ValueError: If `sample_rate` is 0.
        TypeError: If `central_freq` is not of type float.
        TypeError: If `Q` is not of type float.
        ValueError: If `Q` is not in range of (0, 1].
        RuntimeError: If input tensor is not in shape of <..., time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 16])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.BandrejectBiquad(44100, 200.0)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (16,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([16])  # 1 sample
        >>> output = audio.BandrejectBiquad(44100, 200.0)(waveform)
        >>> print(output.shape, output.dtype)
        (16,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_bandreject_biquad
    def __init__(self, sample_rate, central_freq, Q=0.707):
        super().__init__()
        self.sample_rate = sample_rate
        self.central_freq = central_freq
        self.quality_factor = Q

    def parse(self):
        return cde.BandrejectBiquadOperation(self.sample_rate, self.central_freq, self.quality_factor)


class BassBiquad(AudioTensorOperation):
    r"""
    Design a bass tone-control effect, also known as two-pole low-shelf filter for audio waveform.

    A low-shelf filter passes all frequencies, but increase or reduces frequencies below the shelf
    frequency by specified amount. The system function is:

    .. math::
        H(s) = A\frac{s^2 + \frac{\sqrt{A}}{Q}s + A}{As^2 + \frac{\sqrt{A}}{Q}s + 1}

    Similar to `SoX <https://sourceforge.net/projects/sox/>`_ implementation.

    Note:
        The shape of the audio waveform to be processed needs to be <..., time>.

    Args:
        sample_rate (int): Sampling rate (in Hz), which can't be zero.
        gain (float): Desired gain at the boost (or attenuation) in dB.
        central_freq (float, optional): Central frequency (in Hz). Default: ``100.0``.
        Q (float, optional): `Quality factor <https://en.wikipedia.org/wiki/Q_factor>`_ ,
            in range of (0, 1]. Default: ``0.707``.

    Raises:
        TypeError: If `sample_rate` is not of type int.
        ValueError: If `sample_rate` is 0.
        TypeError: If `gain` is not of type float.
        TypeError: If `central_freq` is not of type float.
        TypeError: If `Q` is not of type float.
        ValueError: If `Q` is not in range of (0, 1].
        RuntimeError: If input tensor is not in shape of <..., time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 16])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.BassBiquad(44100, 100.0)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (16,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([16])  # 1 sample
        >>> output = audio.BassBiquad(44100, 200.0)(waveform)
        >>> print(output.shape, output.dtype)
        (16,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_bass_biquad
    def __init__(self, sample_rate, gain, central_freq=100.0, Q=0.707):
        super().__init__()
        self.sample_rate = sample_rate
        self.gain = gain
        self.central_freq = central_freq
        self.quality_factor = Q

    def parse(self):
        return cde.BassBiquadOperation(self.sample_rate, self.gain, self.central_freq, self.quality_factor)


class Biquad(TensorOperation):
    """
    Perform a biquad filter of input audio.
    Mathematical fomulas refer to: `Digital_biquad_filter <https://en.wikipedia.org/wiki/Digital_biquad_filter>`_ .

    Args:
        b0 (float): Numerator coefficient of current input, x[n].
        b1 (float): Numerator coefficient of input one time step ago x[n-1].
        b2 (float): Numerator coefficient of input two time steps ago x[n-2].
        a0 (float): Denominator coefficient of current output y[n], the value can't be 0, typically 1.
        a1 (float): Denominator coefficient of current output y[n-1].
        a2 (float): Denominator coefficient of current output y[n-2].

    Raises:
        TypeError: If `b0` is not of type float.
        TypeError: If `b1` is not of type float.
        TypeError: If `b2` is not of type float.
        TypeError: If `a0` is not of type float.
        TypeError: If `a1` is not of type float.
        TypeError: If `a2` is not of type float.
        ValueError: If `a0` is 0.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 16])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.Biquad(0.01, 0.02, 0.13, 1, 0.12, 0.3)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (16,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([16])  # 1 sample
        >>> output = audio.Biquad(0.01, 0.02, 0.13, 1, 0.12, 0.3)(waveform)
        >>> print(output.shape, output.dtype)
        (16,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_biquad
    def __init__(self, b0, b1, b2, a0, a1, a2):
        super().__init__()
        self.b0 = b0
        self.b1 = b1
        self.b2 = b2
        self.a0 = a0
        self.a1 = a1
        self.a2 = a2

    def parse(self):
        return cde.BiquadOperation(self.b0, self.b1, self.b2, self.a0, self.a1, self.a2)


class ComplexNorm(AudioTensorOperation):
    """
    Compute the norm of complex number sequence.

    Note:
        The shape of the audio waveform to be processed needs to be <..., complex=2>.
        The first dimension represents the real part while the second represents the imaginary.

    Args:
        power (float, optional): Power of the norm, which must be non-negative. Default: ``1.0``.

    Raises:
        TypeError: If `power` is not of type float.
        ValueError: If `power` is a negative number.
        RuntimeError: If input tensor is not in shape of <..., complex=2>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 16, 2])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.ComplexNorm()]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (16,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([16, 2])  # 1 samples
        >>> output = audio.ComplexNorm()(waveform)
        >>> print(output.shape, output.dtype)
        (16,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_complex_norm
    def __init__(self, power=1.0):
        super().__init__()
        self.power = power

    def parse(self):
        return cde.ComplexNormOperation(self.power)


DE_C_BORDER_TYPE = {
    BorderType.CONSTANT: cde.BorderType.DE_BORDER_CONSTANT,
    BorderType.EDGE: cde.BorderType.DE_BORDER_EDGE,
    BorderType.REFLECT: cde.BorderType.DE_BORDER_REFLECT,
    BorderType.SYMMETRIC: cde.BorderType.DE_BORDER_SYMMETRIC,
}


class ComputeDeltas(AudioTensorOperation):
    r"""
    Compute delta coefficients, also known as differential coefficients, of a spectrogram.

    Delta coefficients help to understand the dynamics of the power spectrum. It can be
    computed using the following formula.

    .. math::
        d_{t}=\frac{{\textstyle\sum_{n=1}^{N}}n(c_{t+n}-c_{t-n})}{2{\textstyle\sum_{n=1}^{N}}n^{2}}

    where :math:`d_{t}` is the deltas at time :math:`t` , :math:`c_{t}` is the spectrogram coefficients
    at time :math:`t` , :math:`N` is :math:`(\text{win_length} - 1) // 2` .

    Args:
        win_length (int, optional): The window length used for computing delta, must be no less than 3. Default: ``5``.
        pad_mode (BorderType, optional): Mode parameter passed to padding, can be ``BorderType.CONSTANT``,
            ``BorderType.EDGE``, ``BorderType.REFLECT`` or ``BorderType.SYMMETRIC``. Default: ``BorderType.EDGE``.

            - ``BorderType.CONSTANT``, pad with a constant value.
            - ``BorderType.EDGE``, pad with the last value on the edge.
            - ``BorderType.REFLECT``, reflect the value on the edge while omitting the last one.
              For example, pad [1, 2, 3, 4] with 2 elements on both sides will result in [3, 2, 1, 2, 3, 4, 3, 2].
            - ``BorderType.SYMMETRIC``, reflect the value on the edge while repeating the last one.
              For example, pad [1, 2, 3, 4] with 2 elements on both sides will result in [2, 1, 1, 2, 3, 4, 4, 3].

    Raises:
        TypeError: If `win_length` is not of type int.
        ValueError: If `win_length` is less than 3.
        TypeError: If `pad_mode` is not of type :class:`mindspore.dataset.audio.BorderType` .
        RuntimeError: If input tensor is not in shape of <..., freq, time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 400 // 2 + 1, 30])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.ComputeDeltas(win_length=7, pad_mode=audio.BorderType.EDGE)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (201, 30) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([400 // 2 + 1, 30])  # 1 sample
        >>> output = audio.ComputeDeltas(win_length=7, pad_mode=audio.BorderType.EDGE)(waveform)
        >>> print(output.shape, output.dtype)
        (201, 30) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_compute_deltas
    def __init__(self, win_length=5, pad_mode=BorderType.EDGE):
        super().__init__()
        self.win_len = win_length
        self.pad_mode = pad_mode

    def parse(self):
        return cde.ComputeDeltasOperation(self.win_len, DE_C_BORDER_TYPE.get(self.pad_mode))


class Contrast(AudioTensorOperation):
    """
    Apply contrast effect for audio waveform.

    Comparable with compression, this effect modifies an audio signal to make it sound louder.

    Similar to `SoX <https://sourceforge.net/projects/sox/>`_ implementation.

    Note:
        The shape of the audio waveform to be processed needs to be <..., time>.

    Args:
        enhancement_amount (float, optional): Controls the amount of the enhancement,
            in range of [0, 100]. Default: ``75.0``. Note that `enhancement_amount` equal
            to 0 still gives a significant contrast enhancement.

    Raises:
        TypeError: If `enhancement_amount` is not of type float.
        ValueError: If `enhancement_amount` is not in range [0, 100].
        RuntimeError: If input tensor is not in shape of <..., time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 16])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.Contrast()]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (16,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([16])  # 1 sample
        >>> output = audio.Contrast()(waveform)
        >>> print(output.shape, output.dtype)
        (16,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_contrast
    def __init__(self, enhancement_amount=75.0):
        super().__init__()
        self.enhancement_amount = enhancement_amount

    def parse(self):
        return cde.ContrastOperation(self.enhancement_amount)


class DBToAmplitude(AudioTensorOperation):
    """
    Turn a waveform from the decibel scale to the power/amplitude scale.

    Args:
        ref (float): Reference which the output will be scaled by.
        power (float): If power equals 1, will compute DB to power. If 0.5, will compute DB to amplitude.

    Raises:
        TypeError: If `ref` is not of type float.
        TypeError: If `power` is not of type float.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 16])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.DBToAmplitude(0.5, 0.5)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (16,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([16])  # 1 sample
        >>> output = audio.DBToAmplitude(0.5, 0.5)(waveform)
        >>> print(output.shape, output.dtype)
        (16,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_db_to_amplitude
    def __init__(self, ref, power):
        super().__init__()
        self.ref = ref
        self.power = power

    def parse(self):
        return cde.DBToAmplitudeOperation(self.ref, self.power)


class DCShift(AudioTensorOperation):
    """
    Apply a DC shift to the audio. This can be useful to remove DC offset from audio.

    Args:
        shift (float): The amount to shift the audio, the value must be in the range [-2.0, 2.0].
        limiter_gain (float, optional): Used only on peaks to prevent clipping,
            the value should be much less than 1, such as ``0.05`` or ``0.02``. Default: ``None``,
            will be set to `shift` .

    Raises:
        TypeError: If `shift` is not of type float.
        ValueError: If `shift` is not in range [-2.0, 2.0].
        TypeError: If `limiter_gain` is not of type float.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 16])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.DCShift(0.5, 0.02)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (16,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([16])  # 1 sample
        >>> output = audio.DCShift(0.5, 0.02)(waveform)
        >>> print(output.shape, output.dtype)
        (16,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_dc_shift
    def __init__(self, shift, limiter_gain=None):
        super().__init__()
        self.shift = shift
        self.limiter_gain = limiter_gain if limiter_gain else shift

    def parse(self):
        return cde.DCShiftOperation(self.shift, self.limiter_gain)


class DeemphBiquad(AudioTensorOperation):
    """
    Apply Compact Disc (IEC 60908) de-emphasis (a treble attenuation shelving filter) to the audio waveform.

    Similar to `SoX <https://sourceforge.net/projects/sox/>`_ implementation.

    Args:
        sample_rate (int): Sampling rate of the waveform, must be 44100 or 48000 (Hz).

    Raises:
        TypeError: If `sample_rate` is not of type int.
        ValueError: If `sample_rate` is not 44100 or 48000.
        RuntimeError: If input tensor is not in shape of <..., time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 8])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.DeemphBiquad(44100)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (8,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([8])  # 1 sample
        >>> output = audio.DeemphBiquad(44100)(waveform)
        >>> print(output.shape, output.dtype)
        (8,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_deemph_biquad
    def __init__(self, sample_rate):
        super().__init__()
        self.sample_rate = sample_rate

    def parse(self):
        return cde.DeemphBiquadOperation(self.sample_rate)


class DetectPitchFrequency(AudioTensorOperation):
    """
    Detect pitch frequency.

    It is implemented using normalized cross-correlation function and median smoothing.

    Args:
        sample_rate (int): Sampling rate of the waveform, e.g. ``44100`` (Hz), the value can't be zero.
        frame_time (float, optional): Duration of a frame, the value must be greater than zero. Default: ``0.01``.
        win_length (int, optional): The window length for median smoothing (in number of frames), the value must be
            greater than zero. Default: ``30``.
        freq_low (int, optional): Lowest frequency that can be detected (Hz), the value must be greater than zero.
            Default: ``85``.
        freq_high (int, optional): Highest frequency that can be detected (Hz), the value must be greater than zero.
            Default: ``3400``.

    Raises:
        TypeError: If `sample_rate` is not of type int.
        ValueError: If `sample_rate` is 0.
        TypeError: If `frame_time` is not of type float.
        ValueError: If `frame_time` is not positive.
        TypeError: If `win_length` is not of type int.
        ValueError: If `win_length` is not positive.
        TypeError: If `freq_low` is not of type int.
        ValueError: If `freq_low` is not positive.
        TypeError: If `freq_high` is not of type int.
        ValueError: If `freq_high` is not positive.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 16])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.DetectPitchFrequency(30, 0.1, 3, 5, 25)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (5,) float32
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([16])  # 1 sample
        >>> output = audio.DetectPitchFrequency(30, 0.1, 3, 5, 25)(waveform)
        >>> print(output.shape, output.dtype)
        (5,) float32

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_detect_pitch_frequency
    def __init__(self, sample_rate, frame_time=0.01, win_length=30, freq_low=85, freq_high=3400):
        super().__init__()
        self.sample_rate = sample_rate
        self.frame_time = frame_time
        self.win_length = win_length
        self.freq_low = freq_low
        self.freq_high = freq_high

    def parse(self):
        return cde.DetectPitchFrequencyOperation(self.sample_rate, self.frame_time,
                                                 self.win_length, self.freq_low, self.freq_high)


DE_C_DENSITY_FUNCTION = {DensityFunction.TPDF: cde.DensityFunction.DE_DENSITY_FUNCTION_TPDF,
                         DensityFunction.RPDF: cde.DensityFunction.DE_DENSITY_FUNCTION_RPDF,
                         DensityFunction.GPDF: cde.DensityFunction.DE_DENSITY_FUNCTION_GPDF}


class Dither(AudioTensorOperation):
    """
    Dither increases the perceived dynamic range of audio stored at a
    particular bit-depth by eliminating nonlinear truncation distortion.

    Args:
        density_function (DensityFunction, optional): The density function of a continuous
            random variable, can be ``DensityFunction.TPDF`` (Triangular Probability Density Function),
            ``DensityFunction.RPDF`` (Rectangular Probability Density Function) or
            ``DensityFunction.GPDF`` (Gaussian Probability Density Function).
            Default: ``DensityFunction.TPDF``.
        noise_shaping (bool, optional): A filtering process that shapes the spectral
            energy of quantisation error. Default: ``False``.

    Raises:
        TypeError: If `density_function` is not of type :class:`mindspore.dataset.audio.DensityFunction` .
        TypeError: If `noise_shaping` is not of type bool.
        RuntimeError: If input tensor is not in shape of <..., time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 16])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.Dither()]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (16,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([16])  # 1 sample
        >>> output = audio.Dither()(waveform)
        >>> print(output.shape, output.dtype)
        (16,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_dither
    def __init__(self, density_function=DensityFunction.TPDF, noise_shaping=False):
        super().__init__()
        self.density_function = density_function
        self.noise_shaping = noise_shaping

    def parse(self):
        return cde.DitherOperation(DE_C_DENSITY_FUNCTION.get(self.density_function), self.noise_shaping)


class EqualizerBiquad(AudioTensorOperation):
    """
    Design biquad equalizer filter and perform filtering.

    Similar to `SoX <https://sourceforge.net/projects/sox/>`_ implementation.

    Args:
        sample_rate (int): Sampling rate of the waveform, e.g. ``44100`` (Hz), the value can't be 0.
        center_freq (float): Central frequency (in Hz).
        gain (float): Desired gain at the boost (or attenuation) in dB.
        Q (float, optional): https://en.wikipedia.org/wiki/Q_factor, range: (0, 1]. Default: ``0.707``.

    Raises:
        TypeError: If `sample_rate` is not of type int.
        ValueError: If `sample_rate` is 0.
        TypeError: If `center_freq` is not of type float.
        TypeError: If `gain` is not of type float.
        TypeError: If `Q` is not of type float.
        ValueError: If `Q` is not in range of (0, 1].

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 16])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.EqualizerBiquad(44100, 1500, 5.5, 0.7)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (16,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([16])  # 1 sample
        >>> output = audio.EqualizerBiquad(44100, 1500, 5.5, 0.7)(waveform)
        >>> print(output.shape, output.dtype)
        (16,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_equalizer_biquad
    def __init__(self, sample_rate, center_freq, gain, Q=0.707):
        super().__init__()
        self.sample_rate = sample_rate
        self.center_freq = center_freq
        self.gain = gain
        self.quality_factor = Q

    def parse(self):
        return cde.EqualizerBiquadOperation(self.sample_rate, self.center_freq, self.gain, self.quality_factor)


DE_C_FADE_SHAPE = {FadeShape.QUARTER_SINE: cde.FadeShape.DE_FADE_SHAPE_QUARTER_SINE,
                   FadeShape.HALF_SINE: cde.FadeShape.DE_FADE_SHAPE_HALF_SINE,
                   FadeShape.LINEAR: cde.FadeShape.DE_FADE_SHAPE_LINEAR,
                   FadeShape.LOGARITHMIC: cde.FadeShape.DE_FADE_SHAPE_LOGARITHMIC,
                   FadeShape.EXPONENTIAL: cde.FadeShape.DE_FADE_SHAPE_EXPONENTIAL}


class Fade(AudioTensorOperation):
    """
    Add a fade in and/or fade out to an waveform.

    Args:
        fade_in_len (int, optional): Length of fade-in (time frames), which must be non-negative. Default: ``0``.
        fade_out_len (int, optional): Length of fade-out (time frames), which must be non-negative. Default: ``0``.
        fade_shape (FadeShape, optional): Shape of fade, five different types can be chosen as defined in FadeShape.
            Default: ``FadeShape.LINEAR``.

            - ``FadeShape.QUARTER_SINE``, means it tend to 0 in an quarter sin function.

            - ``FadeShape.HALF_SINE``, means it tend to 0 in an half sin function.

            - ``FadeShape.LINEAR``, means it linear to 0.

            - ``FadeShape.LOGARITHMIC``, means it tend to 0 in an logrithmic function.

            - ``FadeShape.EXPONENTIAL``, means it tend to 0 in an exponential function.

    Raises:
        RuntimeError: If fade_in_len exceeds waveform length.
        RuntimeError: If fade_out_len exceeds waveform length.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 16])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.Fade(fade_in_len=3, fade_out_len=2, fade_shape=audio.FadeShape.LINEAR)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (16,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([16])  # 1 sample
        >>> output = audio.Fade(fade_in_len=3, fade_out_len=2, fade_shape=audio.FadeShape.LINEAR)(waveform)
        >>> print(output.shape, output.dtype)
        (16,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_fade
    def __init__(self, fade_in_len=0, fade_out_len=0, fade_shape=FadeShape.LINEAR):
        super().__init__()
        self.fade_in_len = fade_in_len
        self.fade_out_len = fade_out_len
        self.fade_shape = fade_shape

    def parse(self):
        return cde.FadeOperation(self.fade_in_len, self.fade_out_len, DE_C_FADE_SHAPE.get(self.fade_shape))


class Filtfilt(AudioTensorOperation):
    """
    Apply an IIR filter forward and backward to a waveform.

    Args:
        a_coeffs (Sequence[float]): Denominator coefficients of difference equation of dimension.
            Lower delays coefficients are first, e.g. [a0, a1, a2, ...].
            Must be same size as b_coeffs (pad with 0's as necessary).
        b_coeffs (Sequence[float]): Numerator coefficients of difference equation of dimension.
            Lower delays coefficients are first, e.g. [b0, b1, b2, ...].
            Must be same size as a_coeffs (pad with 0's as necessary).
        clamp (bool, optional): If ``True``, clamp the output signal to be in the range [-1, 1].
            Default: ``True``.

    Raises:
        TypeError: If `a_coeffs` is not of type Sequence[float].
        TypeError: If `b_coeffs` is not of type Sequence[float].
        ValueError: If `a_coeffs` and `b_coeffs` are of different sizes.
        TypeError: If `clamp` is not of type bool.
        RuntimeError: If shape of the input audio is not <..., time>.

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 16])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.Filtfilt(a_coeffs=[0.1, 0.2, 0.3], b_coeffs=[0.1, 0.2, 0.3])]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (16,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([16])  # 1 sample
        >>> output = audio.Filtfilt(a_coeffs=[0.1, 0.2, 0.3], b_coeffs=[0.1, 0.2, 0.3])(waveform)
        >>> print(output.shape, output.dtype)
        (16,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_lfilter
    def __init__(self, a_coeffs, b_coeffs, clamp=True):
        super().__init__()
        self.a_coeffs = a_coeffs
        self.b_coeffs = b_coeffs
        self.clamp = clamp

    def parse(self):
        return cde.FiltfiltOperation(self.a_coeffs, self.b_coeffs, self.clamp)


DE_C_MODULATION = {Modulation.SINUSOIDAL: cde.Modulation.DE_MODULATION_SINUSOIDAL,
                   Modulation.TRIANGULAR: cde.Modulation.DE_MODULATION_TRIANGULAR}

DE_C_INTERPOLATION = {Interpolation.LINEAR: cde.Interpolation.DE_INTERPOLATION_LINEAR,
                      Interpolation.QUADRATIC: cde.Interpolation.DE_INTERPOLATION_QUADRATIC}


class Flanger(AudioTensorOperation):
    """
    Apply a flanger effect to the audio.

    Similar to `SoX <https://sourceforge.net/projects/sox/>`_ implementation.

    Args:
        sample_rate (int): Sampling rate of the waveform, e.g. 44100 (Hz).
        delay (float, optional): Desired delay in milliseconds, in range of [0, 30]. Default: ``0.0``.
        depth (float, optional): Desired delay depth in milliseconds, in range of [0, 10]. Default: ``2.0``.
        regen (float, optional): Desired regen (feedback gain) in dB, in range of [-95, 95]. Default: ``0.0``.
        width (float, optional): Desired width (delay gain) in dB, in range of [0, 100]. Default: ``71.0``.
        speed (float, optional): Modulation speed in Hz, in range of [0.1, 10]. Default: ``0.5``.
        phase (float, optional): Percentage phase-shift for multi-channel, in range of [0, 100]. Default: ``25.0``.
        modulation (Modulation, optional): Modulation method, can be ``Modulation.SINUSOIDAL`` or
            ``Modulation.TRIANGULAR``. Default: ``Modulation.SINUSOIDAL``.
        interpolation (Interpolation, optional): Interpolation method, can be ``Interpolation.LINEAR`` or
            ``Interpolation.QUADRATIC``. Default: ``Interpolation.LINEAR``.

    Raises:
        TypeError: If `sample_rate` is not of type int.
        ValueError: If `sample_rate` is zero.
        TypeError: If `delay` is not of type float.
        ValueError: If `delay` is not in range of [0, 30].
        TypeError: If `depth` is not of type float.
        ValueError: If `depth` is not in range of [0, 10].
        TypeError: If `regen` is not of type float.
        ValueError: If `regen` is not in range of [-95, 95].
        TypeError: If `width` is not of type float.
        ValueError: If `width` is not in range of [0, 100].
        TypeError: If `speed` is not of type float.
        ValueError: If `speed` is not in range of [0.1, 10].
        TypeError: If `phase` is not of type float.
        ValueError: If `phase` is not in range of [0, 100].
        TypeError: If `modulation` is not of type :class:`mindspore.dataset.audio.Modulation` .
        TypeError: If `interpolation` is not of type :class:`mindspore.dataset.audio.Interpolation` .
        RuntimeError: If input tensor is not in shape of <..., channel, time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 4, 16])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.Flanger(44100)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (4, 16) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([4, 16])  # 1 sample
        >>> output = audio.Flanger(44100)(waveform)
        >>> print(output.shape, output.dtype)
        (4, 16) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_flanger
    def __init__(self, sample_rate, delay=0.0, depth=2.0, regen=0.0, width=71.0, speed=0.5, phase=25.0,
                 modulation=Modulation.SINUSOIDAL, interpolation=Interpolation.LINEAR):
        super().__init__()
        self.sample_rate = sample_rate
        self.delay = delay
        self.depth = depth
        self.regen = regen
        self.width = width
        self.speed = speed
        self.phase = phase
        self.modulation = modulation
        self.interpolation = interpolation

    def parse(self):
        return cde.FlangerOperation(self.sample_rate, self.delay, self.depth, self.regen, self.width, self.speed,
                                    self.phase, DE_C_MODULATION.get(self.modulation),
                                    DE_C_INTERPOLATION.get(self.interpolation))


class FrequencyMasking(AudioTensorOperation):
    """
    Apply masking to a spectrogram in the frequency domain.

    Note:
        The shape of the audio waveform to be processed needs to be <..., freq, time>.

    Args:
        iid_masks (bool, optional): Whether to apply different masks to each example/channel. Default: ``False``.
        freq_mask_param (int, optional): When `iid_masks` is ``True``, length of the mask will be uniformly sampled
            from [0, freq_mask_param]; When `iid_masks` is ``False``, directly use it as length of the mask.
            The value should be in range of [0, freq_length], where `freq_length` is the length of audio waveform
            in frequency domain. Default: ``0``.
        mask_start (int, optional): Starting point to apply mask, only works when `iid_masks` is ``True``.
            The value should be in range of [0, freq_length - freq_mask_param], where `freq_length` is
            the length of audio waveform in frequency domain. Default: ``0``.
        mask_value (float, optional): Value to assign to the masked columns. Default: ``0.0``.

    Raises:
        TypeError: If `iid_masks` is not of type bool.
        TypeError: If `freq_mask_param` is not of type int.
        ValueError: If `freq_mask_param` is greater than the length of audio waveform in frequency domain.
        TypeError: If `mask_start` is not of type int.
        ValueError: If `mask_start` is a negative number.
        TypeError: If `mask_value` is not of type float.
        ValueError: If `mask_value` is a negative number.
        RuntimeError: If input tensor is not in shape of <..., freq, time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 16, 2])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.FrequencyMasking(iid_masks=True, freq_mask_param=1)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (16, 2) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([16, 2])  # 1 sample
        >>> output = audio.FrequencyMasking(iid_masks=True, freq_mask_param=1)(waveform)
        >>> print(output.shape, output.dtype)
        (16, 2) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_

    .. image:: frequency_masking_original.png

    .. image:: frequency_masking.png
    """

    @check_masking
    def __init__(self, iid_masks=False, freq_mask_param=0, mask_start=0, mask_value=0.0):
        super().__init__()
        self.iid_masks = iid_masks
        self.frequency_mask_param = freq_mask_param
        self.mask_start = mask_start
        self.mask_value = mask_value

    def parse(self):
        return cde.FrequencyMaskingOperation(self.iid_masks, self.frequency_mask_param, self.mask_start,
                                             self.mask_value)


class Gain(AudioTensorOperation):
    """
    Apply amplification or attenuation to the whole waveform.

    Args:
        gain_db (float): Gain adjustment in decibels (dB). Default: ``1.0``.

    Raises:
        TypeError: If `gain_db` is not of type float.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 8])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.Gain(1.2)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (8,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([8])  # 1 sample
        >>> output = audio.Gain(1.2)(waveform)
        >>> print(output.shape, output.dtype)
        (8,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_gain
    def __init__(self, gain_db=1.0):
        super().__init__()
        self.gain_db = gain_db

    def parse(self):
        return cde.GainOperation(self.gain_db)


class GriffinLim(AudioTensorOperation):
    r"""
    Compute waveform from a linear scale magnitude spectrogram using the Griffin-Lim transformation.

    About Griffin-Lim please refer to `A fast Griffin-Lim algorithm <https://doi.org/10.1109/WASPAA.2013.6701851>`_
    and `Signal estimation from modified short-time Fourier transform <https://doi.org/10.1109/ICASSP.1983.1172092>`_ .

    Args:
        n_fft (int, optional): Size of FFT. Default: ``400``.
        n_iter (int, optional): Number of iteration for phase recovery. Default: ``32``.
        win_length (int, optional): Window size for GriffinLim. Default: ``None``, will be set to `n_fft` .
        hop_length (int, optional): Length of hop between STFT windows.
            Default: ``None``, will be set to `win_length // 2` .
        window_type (WindowType, optional): Window type for GriffinLim, which can be ``WindowType.BARTLETT``,
            ``WindowType.BLACKMAN``, ``WindowType.HAMMING``, ``WindowType.HANN`` or ``WindowType.KAISER``.
            Default: ``WindowType.HANN``. Currently kaiser window is not supported on macOS.
        power (float, optional): Exponent for the magnitude spectrogram. Default: ``2.0``.
        momentum (float, optional): The momentum for fast Griffin-Lim. Default: ``0.99``.
        length (int, optional): Length of the expected output waveform. Default: ``None``,
            will be set to the value of last dimension of the stft matrix.
        rand_init (bool, optional): Flag for random phase initialization or all-zero phase initialization.
            Default: ``True``.

    Raises:
        TypeError: If `n_fft` is not of type int.
        ValueError: If `n_fft` is not positive.
        TypeError: If `n_iter` is not of type int.
        ValueError: If `n_iter` is not positive.
        TypeError: If `win_length` is not of type int.
        ValueError: If `win_length` is a negative number.
        TypeError: If `hop_length` is not of type int.
        ValueError: If `hop_length` is a negative number.
        TypeError: If `window_type` is not of type :class:`mindspore.dataset.audio.WindowType` .
        TypeError: If `power` is not of type float.
        ValueError: If `power` is not positive.
        TypeError: If `momentum` is not of type float.
        ValueError: If `momentum` is a negative number.
        TypeError: If `length` is not of type int.
        ValueError: If `length` is a negative number.
        TypeError: If `rand_init` is not of type bool.
        RuntimeError: If `n_fft` is not less than `length` .
        RuntimeError: If `win_length` is not less than `n_fft` .

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 201, 6])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.GriffinLim(n_fft=400)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (1000,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([201, 6])  # 1 sample
        >>> output = audio.GriffinLim(n_fft=400)(waveform)
        >>> print(output.shape, output.dtype)
        (1000,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_griffin_lim
    def __init__(self, n_fft=400, n_iter=32, win_length=None, hop_length=None, window_type=WindowType.HANN, power=2.0,
                 momentum=0.99, length=None, rand_init=True):
        super().__init__()
        self.n_fft = n_fft
        self.n_iter = n_iter
        self.win_length = win_length if win_length else self.n_fft
        self.hop_length = hop_length if hop_length else self.win_length // 2
        self.window_type = window_type
        self.power = power
        self.momentum = momentum
        self.length = length if length else 0
        self.rand_init = rand_init

    def parse(self):
        return cde.GriffinLimOperation(self.n_fft, self.n_iter, self.win_length, self.hop_length,
                                       DE_C_WINDOW_TYPE.get(self.window_type), self.power, self.momentum, self.length,
                                       self.rand_init)


class HighpassBiquad(AudioTensorOperation):
    """
    Design biquad highpass filter and perform filtering.

    Similar to `SoX <https://sourceforge.net/projects/sox/>`_ implementation.

    Args:
        sample_rate (int): Sampling rate of the waveform, e.g. 44100 (Hz), the value can't be 0.
        cutoff_freq (float): Filter cutoff frequency (in Hz).
        Q (float, optional): Quality factor, https://en.wikipedia.org/wiki/Q_factor, range: (0, 1]. Default: ``0.707``.

    Raises:
        TypeError: If `sample_rate` is not of type int.
        ValueError: If `sample_rate` is 0.
        TypeError: If `cutoff_freq` is not of type float.
        TypeError: If `Q` is not of type float.
        ValueError: If `Q` is not in range of (0, 1].
        RuntimeError: If the shape of input audio waveform does not match <..., time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 16])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.HighpassBiquad(44100, 1500, 0.7)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (16,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([16])  # 1 sample
        >>> output = audio.HighpassBiquad(44100, 1500, 0.7)(waveform)
        >>> print(output.shape, output.dtype)
        (16,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_highpass_biquad
    def __init__(self, sample_rate, cutoff_freq, Q=0.707):
        super().__init__()
        self.sample_rate = sample_rate
        self.cutoff_freq = cutoff_freq
        self.quality_factor = Q

    def parse(self):
        return cde.HighpassBiquadOperation(self.sample_rate, self.cutoff_freq, self.quality_factor)


class InverseMelScale(AudioTensorOperation):
    """
    Solve for a normal STFT from a mel frequency STFT, using a conversion matrix.

    Args:
        n_stft (int): Number of bins in STFT.
        n_mels (int, optional): Number of mel filterbanks. Default: ``128``.
        sample_rate (int, optional): Sample rate of audio signal. Default: ``16000``.
        f_min (float, optional): Minimum frequency. Default: ``0.0``.
        f_max (float, optional): Maximum frequency. Default: ``None``, will be set to `sample_rate // 2` .
        max_iter (int, optional): Maximum number of optimization iterations. Default: ``100000``.
        tolerance_loss (float, optional): Value of loss to stop optimization at. Default: ``1e-5``.
        tolerance_change (float, optional): Difference in losses to stop optimization at. Default: ``1e-8``.
        sgdargs (dict, optional): Arguments for the SGD optimizer. Default: ``None``, will be set to
            {'sgd_lr': 0.1, 'sgd_momentum': 0.9}.
        norm (NormType, optional): Normalization method, can be ``NormType.SLANEY`` or ``NormType.NONE``.
            Default: ``NormType.NONE``, no narmalization.
        mel_type (MelType, optional): Mel scale to use, can be ``MelType.SLANEY`` or ``MelType.HTK``.
            Default: ``MelType.HTK``.

    Raises:
        TypeError: If `n_stft` is not of type int.
        ValueError: If `n_stft` is not positive.
        TypeError: If `n_mels` is not of type int.
        ValueError: If `n_mels` is not positive.
        TypeError: If `sample_rate` is not of type int.
        ValueError: If `sample_rate` is not positive.
        TypeError: If `f_min` is not of type float.
        ValueError: If `f_min` is greater than or equal to `f_max` .
        TypeError: If `f_max` is not of type float.
        ValueError: If `f_max` is a negative number.
        TypeError: If `max_iter` is not of type int.
        ValueError: If `max_iter` is a negative number.
        TypeError: If `tolerance_loss` is not of type float.
        ValueError: If `tolerance_loss` is a negative number.
        TypeError: If `tolerance_change` is not of type float.
        ValueError: If `tolerance_change` is a negative number.
        TypeError: If `sgdargs` is not of type dict.
        TypeError: If `norm` is not of type  :class:`mindspore.dataset.audio.NormType` .
        TypeError: If `mel_type` is not of type  :class:`mindspore.dataset.audio.MelType` .

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.randn(5, 8, 3, 2)  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.InverseMelScale(20, 3, 16000, 0, 8000, 10)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (8, 20, 2) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([8, 3, 2])  # 1 sample
        >>> output = audio.InverseMelScale(20, 3, 16000, 0, 8000, 10)(waveform)
        >>> print(output.shape, output.dtype)
        (8, 20, 2) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_inverse_mel_scale
    def __init__(self, n_stft, n_mels=128, sample_rate=16000, f_min=0.0, f_max=None, max_iter=100000,
                 tolerance_loss=1e-5, tolerance_change=1e-8, sgdargs=None, norm=NormType.NONE, mel_type=MelType.HTK):
        super().__init__()
        self.n_stft = n_stft
        self.n_mels = n_mels
        self.sample_rate = sample_rate
        self.f_min = f_min
        self.f_max = f_max if f_max is not None else sample_rate // 2
        self.max_iter = max_iter
        self.tolerance_loss = tolerance_loss
        self.tolerance_change = tolerance_change
        if sgdargs is None:
            self.sgdargs = {'sgd_lr': 0.1, 'sgd_momentum': 0.9}
        else:
            self.sgdargs = sgdargs
        self.norm = norm
        self.mel_type = mel_type

    def parse(self):
        return cde.InverseMelScaleOperation(self.n_stft, self.n_mels, self.sample_rate, self.f_min, self.f_max,
                                            self.max_iter, self.tolerance_loss, self.tolerance_change, self.sgdargs,
                                            DE_C_NORM_TYPE.get(self.norm), DE_C_MEL_TYPE.get(self.mel_type))


class InverseSpectrogram(AudioTensorOperation):
    """
    Create an inverse spectrogram to recover an audio signal from a spectrogram.

    Args:
        length (int, optional): The output length of the waveform, must be non negative. Default: ``None``,
            means to output the whole waveform.
        n_fft (int, optional): Size of FFT, creates `n_fft // 2 + 1` bins, which should be greater than 0.
            Default: ``400``.
        win_length (int, optional): Window size, which should be greater than 0.
            Default: ``None``, will be set to `n_fft` .
        hop_length (int, optional): Length of hop between STFT windows, which should be greater than 0.
            Default: ``None``, will be set to `win_length // 2` .
        pad (int, optional): Two sided padding of signal, cannot be less than 0. Default: ``0``.
        window (WindowType, optional): A function to create a window tensor that is applied/multiplied to each
            frame/window. Default: ``WindowType.HANN``.
        normalized (bool, optional): Whether the spectrogram was normalized by magnitude after stft. Default: ``False``.
        center (bool, optional): Whether the signal in spectrogram was padded on both sides. Default: ``True``.
        pad_mode (BorderType, optional): Controls the padding method used when `center` is ``True``,
            can be ``BorderType.REFLECT``, ``BorderType.CONSTANT``, ``BorderType.EDGE`` or ``BorderType.SYMMETRIC``.
            Default: ``BorderType.REFLECT``.
        onesided (bool, optional): Controls whether spectrogram was used to return half of results to avoid
            redundancy. Default: ``True``.

    Raises:
        TypeError: If `length` is not of type int.
        ValueError: If `length` is a negative number.
        TypeError: If `n_fft` is not of type int.
        ValueError: If `n_fft` is not positive.
        TypeError: If `win_length` is not of type int.
        ValueError: If `win_length` is not positive.
        TypeError: If `hop_length` is not of type int.
        ValueError: If `hop_length` is not positive.
        TypeError: If `pad` is not of type int.
        ValueError: If `pad` is a negative number.
        TypeError: If `window` is not of type :class:`mindspore.dataset.audio.WindowType` .
        TypeError: If `normalized` is not of type bool.
        TypeError: If `center` is not of type bool.
        TypeError: If `pad_mode` is not of type :class:`mindspore.dataset.audio.BorderType` .
        TypeError: If `onesided` is not of type bool.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 400 // 2 + 1, 30, 2])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.InverseSpectrogram(1, 400, 400, 200)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (1,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([400 // 2 + 1, 30, 2])  # 1 sample
        >>> output = audio.InverseSpectrogram(1, 400, 400, 200)(waveform)
        >>> print(output.shape, output.dtype)
        (1,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_inverse_spectrogram
    def __init__(self, length=None, n_fft=400, win_length=None, hop_length=None, pad=0,
                 window=WindowType.HANN, normalized=False, center=True,
                 pad_mode=BorderType.REFLECT, onesided=True):
        super().__init__()
        self.length = length if length is not None else 0
        self.n_fft = n_fft
        self.win_length = win_length if win_length is not None else n_fft
        self.hop_length = hop_length if hop_length is not None else self.win_length // 2
        self.pad = pad
        self.window = window
        self.normalized = normalized
        self.center = center
        self.pad_mode = pad_mode
        self.onesided = onesided

    def parse(self):
        return cde.InverseSpectrogramOperation(self.length, self.n_fft, self.win_length, self.hop_length, self.pad,
                                               DE_C_WINDOW_TYPE.get(self.window), self.normalized, self.center,
                                               DE_C_BORDER_TYPE.get(self.pad_mode), self.onesided)


DE_C_NORM_MODE = {NormMode.ORTHO: cde.NormMode.DE_NORM_MODE_ORTHO,
                  NormMode.NONE: cde.NormMode.DE_NORM_MODE_NONE}


class LFCC(AudioTensorOperation):
    """
    Create LFCC for a raw audio signal.

    Note:
        The shape of the audio waveform to be processed needs to be <..., time>.

    Args:
        sample_rate (int, optional): Sample rate of audio signal. Default: ``16000``.
        n_filter (int, optional) : Number of linear filters to apply. Default: ``128``.
        n_lfcc (int, optional) : Number of lfc coefficients to retain. Default: ``40``.
        f_min (float, optional): Minimum frequency. Default: ``0.0``.
        f_max (float, optional): Maximum frequency. Default: ``None``, will be set to `sample_rate // 2` .
        dct_type (int, optional) : Type of DCT to use. The value can only be ``2``. Default: ``2``.
        norm (NormMode, optional) : Norm to use. Default: ``NormMode.ORTHO``.
        log_lf (bool, optional) : Whether to use log-lf spectrograms instead of db-scaled. Default: ``False``.
        speckwargs (dict, optional) : Arguments for :class:`mindspore.dataset.audio.Spectrogram`.
            Default: ``None``, the default setting is a dict including

            - 'n_fft': 400
            - 'win_length': n_fft
            - 'hop_length': win_length // 2
            - 'pad': 0
            - 'window': WindowType.HANN
            - 'power': 2.0
            - 'normalized': False
            - 'center': True
            - 'pad_mode': BorderType.REFLECT
            - 'onesided': True

    Raises:
        TypeError: If `sample_rate` is not of type int.
        TypeError: If `n_filter` is not of type int.
        TypeError: If `n_lfcc` is not of type int.
        TypeError: If `norm` is not of type :class:`mindspore.dataset.audio.NormMode` .
        TypeError: If `log_lf` is not of type bool.
        TypeError: If `speckwargs` is not of type dict.
        ValueError: If `sample_rate` is 0.
        ValueError: If `n_lfcc` is less than 0.
        ValueError: If `f_min` is greater than `f_max` .
        ValueError: If `f_min` is greater than `sample_rate // 2` when `f_max` is set to None.
        ValueError: If `dct_type` is not ``2``.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 10, 300])
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.LFCC()]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (10, 40, 2) float32
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([10, 300])  # 1 sample
        >>> output = audio.LFCC()(waveform)
        >>> print(output.shape, output.dtype)
        (10, 40, 2) float32

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_lfcc
    def __init__(self, sample_rate=16000, n_filter=128, n_lfcc=40, f_min=0.0, f_max=None, dct_type=2,
                 norm=NormMode.ORTHO, log_lf=False, speckwargs=None):
        super().__init__()
        self.sample_rate = sample_rate
        self.n_filter = n_filter
        self.n_lfcc = n_lfcc
        self.f_min = f_min
        self.f_max = f_max if f_max is not None else sample_rate // 2
        self.dct_type = dct_type
        self.norm = norm
        self.log_lf = log_lf
        self.speckwargs = speckwargs
        if speckwargs is None:
            self.speckwargs = {}
        self.speckwargs.setdefault("n_fft", 400)
        self.speckwargs.setdefault("win_length", self.speckwargs.get("n_fft"))
        self.speckwargs.setdefault("hop_length", self.speckwargs.get("win_length") // 2)
        self.speckwargs.setdefault("pad", 0)
        self.speckwargs.setdefault("window", WindowType.HANN)
        self.speckwargs.setdefault("power", 2.0)
        self.speckwargs.setdefault("normalized", False)
        self.speckwargs.setdefault("center", True)
        self.speckwargs.setdefault("pad_mode", BorderType.REFLECT)
        self.speckwargs.setdefault("onesided", True)
        self.window = self.speckwargs.get("window")
        self.pad_mode = self.speckwargs.get("pad_mode")

    def parse(self):
        return cde.LFCCOperation(self.sample_rate, self.n_filter, self.n_lfcc, self.f_min, self.f_max,
                                 self.dct_type, DE_C_NORM_MODE.get(self.norm), self.log_lf, self.speckwargs,
                                 DE_C_WINDOW_TYPE.get(self.window), DE_C_BORDER_TYPE.get(self.pad_mode))


class LFilter(AudioTensorOperation):
    """
    Perform an IIR filter by evaluating different equation.

    Args:
        a_coeffs (Sequence[float]): Denominator coefficients of difference equation of dimension.
            Lower delays coefficients are first, e.g. [a0, a1, a2, ...].
            Must be same size as b_coeffs (pad with 0's as necessary).
        b_coeffs (Sequence[float]): Numerator coefficients of difference equation of dimension.
            Lower delays coefficients are first, e.g. [b0, b1, b2, ...].
            Must be same size as a_coeffs (pad with 0's as necessary).
        clamp (bool, optional): If True, clamp the output signal to be in the range [-1, 1]. Default: ``True``.

    Raises:
        TypeError: If `a_coeffs` is not of type Sequence[float].
        TypeError: If `b_coeffs` is not of type Sequence[float].
        ValueError: If `a_coeffs` and `b_coeffs` are of different sizes.
        TypeError: If `clamp` is not of type bool.
        RuntimeError: If input tensor is not in shape of <..., time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 16])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.LFilter(a_coeffs=[0.1, 0.2, 0.3], b_coeffs=[0.3, 0.2, 0.1])]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (16,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([16])  # 1 sample
        >>> output = audio.LFilter(a_coeffs=[0.1, 0.2, 0.3], b_coeffs=[0.3, 0.2, 0.1])(waveform)
        >>> print(output.shape, output.dtype)
        (16,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_lfilter
    def __init__(self, a_coeffs, b_coeffs, clamp=True):
        super().__init__()
        self.a_coeffs = a_coeffs
        self.b_coeffs = b_coeffs
        self.clamp = clamp

    def parse(self):
        return cde.LFilterOperation(self.a_coeffs, self.b_coeffs, self.clamp)


class LowpassBiquad(AudioTensorOperation):
    r"""
    Design two-pole low-pass filter for audio waveform.

    A low-pass filter passes frequencies lower than a selected cutoff frequency
    but attenuates frequencies higher than it. The system function is:

    .. math::
        H(s) = \frac{1}{s^2 + \frac{s}{Q} + 1}

    Similar to `SoX <https://sourceforge.net/projects/sox/>`_ implementation.

    Note:
        The shape of the audio waveform to be processed needs to be <..., time>.

    Args:
        sample_rate (int): Sampling rate (in Hz), which can't be zero.
        cutoff_freq (float): Filter cutoff frequency (in Hz).
        Q (float, optional): `Quality factor <https://en.wikipedia.org/wiki/Q_factor>`_ ,
            in range of (0, 1]. Default: ``0.707``.

    Raises:
        TypeError: If `sample_rate` is not of type int.
        ValueError: If `sample_rate` is 0.
        TypeError: If `cutoff_freq` is not of type float.
        TypeError: If `Q` is not of type float.
        ValueError: If `Q` is not in range of (0, 1].
        RuntimeError: If input tensor is not in shape of <..., time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 10])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.LowpassBiquad(4000, 1500, 0.7)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (10,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([10])  # 1 sample
        >>> output = audio.LowpassBiquad(4000, 1500, 0.7)(waveform)
        >>> print(output.shape, output.dtype)
        (10,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_lowpass_biquad
    def __init__(self, sample_rate, cutoff_freq, Q=0.707):
        super().__init__()
        self.sample_rate = sample_rate
        self.cutoff_freq = cutoff_freq
        self.quality_factor = Q

    def parse(self):
        return cde.LowpassBiquadOperation(self.sample_rate, self.cutoff_freq, self.quality_factor)


class Magphase(AudioTensorOperation):
    """
    Separate a complex-valued spectrogram with shape :math:`(..., 2)` into its magnitude and phase.

    Args:
        power (float): Power of the norm, which must be non-negative. Default: ``1.0``.

    Raises:
        RuntimeError: If the shape of input audio waveform does not match (..., 2).

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 16, 2])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.Magphase()]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"],
        ...                                                 output_columns=["spect", "phase"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["spect"].shape, item["spect"].dtype)
        ...     break
        (16,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([16, 2])  # 1 sample
        >>> output = audio.Magphase()(waveform)
        >>> print(output[0].shape, output[0].dtype)
        (16,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_magphase
    def __init__(self, power=1.0):
        super().__init__()
        self.power = power

    def parse(self):
        return cde.MagphaseOperation(self.power)


class MaskAlongAxis(AudioTensorOperation):
    """
    Apply a mask along `axis` . Mask will be applied from indices `[mask_start, mask_start + mask_width)` .

    Args:
        mask_start (int): Starting position of the mask, which must be non negative.
        mask_width (int): The width of the mask, which must be larger than 0.
        mask_value (float): Value to assign to the masked columns.
        axis (int): Axis to apply mask on (1 for frequency and 2 for time).

    Raises:
        ValueError: If `mask_start` is invalid (< 0).
        ValueError: If `mask_width` is invalid (< 1).
        ValueError: If `axis` is not type of int or not within [1, 2].

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 20, 20])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.MaskAlongAxis(0, 10, 0.5, 1)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (20, 20) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([20, 20])  # 1 sample
        >>> output = audio.MaskAlongAxis(0, 10, 0.5, 1)(waveform)
        >>> print(output.shape, output.dtype)
        (20, 20) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_mask_along_axis
    def __init__(self, mask_start, mask_width, mask_value, axis):
        super().__init__()
        self.mask_start = mask_start
        self.mask_width = mask_width
        self.mask_value = mask_value
        self.axis = axis

    def parse(self):
        return cde.MaskAlongAxisOperation(self.mask_start, self.mask_width, self.mask_value, self.axis)


class MaskAlongAxisIID(AudioTensorOperation):
    """
    Apply a mask along `axis` . Mask will be applied from indices `[mask_start, mask_start + mask_width)` , where
    `mask_width` is sampled from `uniform[0, mask_param]` , and `mask_start` from
    `uniform[0, max_length - mask_width]` , `max_length` is the number of columns of the specified axis
    of the spectrogram.

    Args:
        mask_param (int): Number of columns to be masked, will be uniformly sampled from
            [0, mask_param], must be non negative.
        mask_value (float): Value to assign to the masked columns.
        axis (int): Axis to apply mask on (1 for frequency and 2 for time).

    Raises:
        TypeError: If `mask_param` is not of type int.
        ValueError: If `mask_param` is a negative value.
        TypeError: If `mask_value` is not of type float.
        TypeError: If `axis` is not of type int.
        ValueError: If `axis` is not in range of [1, 2].
        RuntimeError: If input tensor is not in shape of <..., freq, time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform= np.random.random([5, 20, 20])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.MaskAlongAxisIID(5, 0.5, 2)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (20, 20) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([20, 20])  # 1 sample
        >>> output = audio.MaskAlongAxisIID(5, 0.5, 2)(waveform)
        >>> print(output.shape, output.dtype)
        (20, 20) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_mask_along_axis_iid
    def __init__(self, mask_param, mask_value, axis):
        super().__init__()
        self.mask_param = mask_param
        self.mask_value = mask_value
        self.axis = axis

    def parse(self):
        return cde.MaskAlongAxisIIDOperation(self.mask_param, self.mask_value, self.axis)


DE_C_MEL_TYPE = {MelType.SLANEY: cde.MelType.DE_MEL_TYPE_SLANEY,
                 MelType.HTK: cde.MelType.DE_MEL_TYPE_HTK}

DE_C_NORM_TYPE = {NormType.NONE: cde.NormType.DE_NORM_TYPE_NONE,
                  NormType.SLANEY: cde.NormType.DE_NORM_TYPE_SLANEY}


class MelScale(AudioTensorOperation):
    """
    Convert normal STFT to STFT at the Mel scale.

    Args:
        n_mels (int, optional): Number of mel filterbanks. Default: ``128``.
        sample_rate (int, optional): Sample rate of audio signal. Default: ``16000``.
        f_min (float, optional): Minimum frequency. Default: ``0.0``.
        f_max (float, optional): Maximum frequency. Default: ``None``, will be set to `sample_rate // 2` .
        n_stft (int, optional): Number of bins in STFT. Default: ``201``.
        norm (NormType, optional): Type of norm, value should be ``NormType.SLANEY`` or ``NormType.NONE``.
            If `norm` is ``NormType.SLANEY``, divide the triangular mel weight by the width of the mel band.
            Default: ``NormType.NONE``, no narmalization.
        mel_type (MelType, optional): Type to use, value should be ``MelType.SLANEY`` or ``MelType.HTK``.
            Default: ``MelType.HTK``.

    Raises:
        TypeError: If `n_mels` is not of type int.
        ValueError: If `n_mels` is not positive.
        TypeError: If `sample_rate` is not of type int.
        ValueError: If `sample_rate` is not positive.
        TypeError: If `f_min` is not of type float.
        ValueError: If `f_min` is greater than or equal to `f_max` .
        TypeError: If `f_max` is not of type float.
        ValueError: If `f_max` is a negative number.
        TypeError: If `n_stft` is not of type int.
        ValueError: If `n_stft` is not positive.
        TypeError: If `norm` is not of type  :class:`mindspore.dataset.audio.NormType` .
        TypeError: If `mel_type` is not of type  :class:`mindspore.dataset.audio.MelType` .

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 201, 3])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.MelScale(200, 1500, 0.7)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (200, 3) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([201, 3])  # 1 sample
        >>> output = audio.MelScale(200, 1500, 0.7)(waveform)
        >>> print(output.shape, output.dtype)
        (200, 3) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_mel_scale
    def __init__(self, n_mels=128, sample_rate=16000, f_min=0.0, f_max=None, n_stft=201, norm=NormType.NONE,
                 mel_type=MelType.HTK):
        super().__init__()
        self.n_mels = n_mels
        self.sample_rate = sample_rate
        self.f_min = f_min
        self.f_max = f_max if f_max is not None else sample_rate // 2
        self.n_stft = n_stft
        self.norm = norm
        self.mel_type = mel_type

    def parse(self):
        return cde.MelScaleOperation(self.n_mels, self.sample_rate, self.f_min, self.f_max, self.n_stft,
                                     DE_C_NORM_TYPE.get(self.norm), DE_C_MEL_TYPE.get(self.mel_type))


class MelSpectrogram(AudioTensorOperation):
    r"""
    Create MelSpectrogram for a raw audio signal.

    Args:
        sample_rate (int, optional): Sampling rate of audio signal (in Hz), which can't be less than 0.
            Default: ``16000``.
        n_fft (int, optional): Size of FFT, creates `n_fft // 2 + 1` bins, which should be greater than 0 and less than
            twice of the last dimension size of the input. Default: ``400``.
        win_length (int, optional): Window size, which should be greater than 0 and no more than `n_fft` . Default:
            None, will be set to `n_fft` .
        hop_length (int, optional): Length of hop between STFT windows, which should be greater than 0.
            Default: ``None``, will be set to `win_length // 2` .
        f_min (float, optional): Minimum frequency, which can't be greater than `f_max` . Default: ``0.0``.
        f_max (float, optional): Maximum frequency, which can't be less than 0. Default: ``None``, will be set
            to `sample_rate // 2` .
        pad (int, optional): Two sided padding of signal, which can't be less than 0. Default: ``0``.
        n_mels (int, optional): Number of mel filterbanks, which can't be less than 0. Default: ``128``.
        window (WindowType, optional): A function to create a window tensor that is applied/multiplied to each
            frame/window. Default: ``WindowType.HANN``.
        power (float, optional): Exponent for the magnitude spectrogram, which must be
            greater than 0, e.g., ``1`` for energy, ``2`` for power, etc. Default: ``2.0``.
        normalized (bool, optional): Whether to normalize by magnitude after stft. Default: ``False``.
        center (bool, optional): Whether to pad waveform on both sides. Default: ``True``.
        pad_mode (BorderType, optional): Controls the padding method used when `center` is ``True``,
            can be ``BorderType.REFLECT``, ``BorderType.CONSTANT``, ``BorderType.EDGE`` or ``BorderType.SYMMETRIC``.
            Default: ``BorderType.REFLECT``.
        onesided (bool, optional): Controls whether to return half of results to avoid redundancy. Default: ``True``.
        norm (NormType, optional): If 'slaney', divide the triangular mel weights by the width of the mel band
            (area normalization). Default: ``NormType.NONE``, no narmalization.
        mel_scale (MelType, optional): Mel scale to use, can be ``MelType.SLANEY`` or ``MelType.HTK``.
            Default: ``MelType.HTK``.

    Raises:
        TypeError: If `sample_rate` is not of type int.
        TypeError: If `n_fft` is not of type int.
        TypeError: If `n_mels` is not of type int.
        TypeError: If `f_min` is not of type float.
        TypeError: If `f_max` is not of type float.
        TypeError: If `window` is not of type :class:`mindspore.dataset.audio.WindowType` .
        TypeError: If `norm` is not of type :class:`mindspore.dataset.audio.NormType` .
        TypeError: If `mel_scale` is not of type :class:`mindspore.dataset.audio.MelType` .
        TypeError: If `power` is not of type float.
        TypeError: If `normalized` is not of type bool.
        TypeError: If `center` is not of type bool.
        TypeError: If `pad_mode` is not of type :class:`mindspore.dataset.audio.BorderType` .
        TypeError: If `onesided` is not of type bool.
        TypeError: If `pad` is not of type int.
        TypeError: If `win_length` is not of type int.
        TypeError: If `hop_length` is not of type int.
        ValueError: If `sample_rate` is a negative number.
        ValueError: If `n_fft` is not positive.
        ValueError: If `n_mels` is a negative number.
        ValueError: If `f_min` is greater than `f_max` .
        ValueError: If `f_max` is a negative number.
        ValueError: If `f_min` is not less than `sample_rate // 2` when `f_max` is set to None.
        ValueError: If `power` is not positive.
        ValueError: If `pad` is a negative number.
        ValueError: If `win_length` is not positive.
        ValueError: If `hop_length` is not positive.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 32])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.MelSpectrogram(sample_rate=16000, n_fft=16, win_length=16, hop_length=8, f_min=0.0,
        ...                                    f_max=5000.0, pad=0, n_mels=2, window=audio.WindowType.HANN, power=2.0,
        ...                                    normalized=False, center=True, pad_mode=audio.BorderType.REFLECT,
        ...                                    onesided=True, norm=audio.NormType.SLANEY, mel_scale=audio.MelType.HTK)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (2, 5) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([32])  # 1 sample
        >>> output = audio.MelSpectrogram(sample_rate=16000, n_fft=16, win_length=16, hop_length=8, f_min=0.0,
        ...                               f_max=5000.0, pad=0, n_mels=2, window=audio.WindowType.HANN, power=2.0,
        ...                               normalized=False, center=True, pad_mode=audio.BorderType.REFLECT,
        ...                               onesided=True, norm=audio.NormType.SLANEY,
        ...                               mel_scale=audio.MelType.HTK)(waveform)
        >>> print(output.shape, output.dtype)
        (2, 5) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_mel_spectrogram
    def __init__(self, sample_rate=16000, n_fft=400, win_length=None, hop_length=None, f_min=0.0, f_max=None, pad=0,
                 n_mels=128, window=WindowType.HANN, power=2.0, normalized=False, center=True,
                 pad_mode=BorderType.REFLECT, onesided=True, norm=NormType.NONE, mel_scale=MelType.HTK):
        super().__init__()
        self.sample_rate = sample_rate
        self.n_fft = n_fft
        self.win_length = win_length if win_length is not None else n_fft
        self.hop_length = hop_length if hop_length is not None else self.win_length // 2
        self.f_min = f_min
        self.f_max = f_max if f_max is not None else sample_rate // 2
        self.pad = pad
        self.n_mels = n_mels
        self.window = window
        self.power = power
        self.normalized = normalized
        self.center = center
        self.pad_mode = pad_mode
        self.onesided = onesided
        self.norm = norm
        self.mel_scale = mel_scale

    def parse(self):
        return cde.MelSpectrogramOperation(self.sample_rate, self.n_fft, self.win_length, self.hop_length, self.f_min,
                                           self.f_max, self.pad, self.n_mels, DE_C_WINDOW_TYPE.get(self.window),
                                           self.power, self.normalized, self.center,
                                           DE_C_BORDER_TYPE.get(self.pad_mode), self.onesided,
                                           DE_C_NORM_TYPE.get(self.norm), DE_C_MEL_TYPE.get(self.mel_scale))


class MFCC(AudioTensorOperation):
    """
    Create MFCC for a raw audio signal.

    Args:
        sample_rate (int, optional): Sampling rate of audio signal (in Hz), can't be less than 0. Default: ``16000``.
        n_mfcc (int, optional): Number of mfc coefficients to retain, can't be less than 0. Default: ``40``.
        dct_type (int, optional): Type of DCT (discrete cosine transform) to use, can only be ``2``. Default: ``2``.
        norm (NormMode, optional): Norm to use. Default: ``NormMode.ORTHO``.
        log_mels (bool, optional): Whether to use log-mel spectrograms instead of db-scaled. Default: ``False``.
        melkwargs (dict, optional): Arguments for :class:`mindspore.dataset.audio.MelSpectrogram`.
            Default: ``None``, the default setting is a dict including

            - 'n_fft': 400
            - 'win_length': n_fft
            - 'hop_length': win_length // 2
            - 'f_min': 0.0
            - 'f_max': sample_rate // 2
            - 'pad': 0
            - 'window': WindowType.HANN
            - 'power': 2.0
            - 'normalized': False
            - 'center': True
            - 'pad_mode': BorderType.REFLECT
            - 'onesided': True
            - 'norm': NormType.NONE
            - 'mel_scale': MelType.HTK

    Raises:
        TypeError: If `sample_rate` is not of type int.
        TypeError: If `log_mels` is not of type bool.
        TypeError: If `norm` is not of type :class:`mindspore.dataset.audio.NormMode` .
        TypeError: If `n_mfcc` is not of type int.
        TypeError: If `melkwargs` is not of type dict.
        ValueError: If `sample_rate` is a negative number.
        ValueError: If `n_mfcc` is a negative number.
        ValueError: If `dct_type` is not ``2``.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 500])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.MFCC(4000, 128, 2)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (128, 3) float32
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([500])  # 1 sample
        >>> output = audio.MFCC(4000, 128, 2)(waveform)
        >>> print(output.shape, output.dtype)
        (128, 3) float32

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_mfcc
    def __init__(self, sample_rate=16000, n_mfcc=40, dct_type=2, norm=NormMode.ORTHO, log_mels=False, melkwargs=None):
        super().__init__()
        self.sample_rate = sample_rate
        self.n_mfcc = n_mfcc
        self.dct_type = dct_type
        self.norm = norm
        self.log_mels = log_mels
        self.melkwargs = melkwargs
        if melkwargs is None:
            self.melkwargs = {}
        self.melkwargs.setdefault("n_fft", 400)
        self.melkwargs.setdefault("win_length", self.melkwargs.get("n_fft"))
        self.melkwargs.setdefault("hop_length", self.melkwargs.get("win_length") // 2)
        self.melkwargs.setdefault("f_min", 0.0)
        self.melkwargs.setdefault("f_max", sample_rate // 2)
        self.melkwargs.setdefault("pad", 0)
        self.melkwargs.setdefault("n_mels", 128)
        self.melkwargs.setdefault("window", WindowType.HANN)
        self.melkwargs.setdefault("power", 2.0)
        self.melkwargs.setdefault("normalized", False)
        self.melkwargs.setdefault("center", True)
        self.melkwargs.setdefault("pad_mode", BorderType.REFLECT)
        self.melkwargs.setdefault("onesided", True)
        self.melkwargs.setdefault("norm", NormType.NONE)
        self.melkwargs.setdefault("mel_scale", MelType.HTK)
        self.window = self.melkwargs.get("window")
        self.pad_mode = self.melkwargs.get("pad_mode")
        self.norm_mel = self.melkwargs.get("norm")
        self.mel_scale = self.melkwargs.get("mel_scale")

    def parse(self):
        return cde.MFCCOperation(self.sample_rate, self.n_mfcc, self.dct_type, DE_C_NORM_MODE.get(self.norm),
                                 self.log_mels, self.melkwargs, DE_C_WINDOW_TYPE.get(self.window),
                                 DE_C_BORDER_TYPE.get(self.pad_mode), DE_C_NORM_TYPE.get(self.norm_mel),
                                 DE_C_MEL_TYPE.get(self.mel_scale))


class MuLawDecoding(AudioTensorOperation):
    """
    Decode mu-law encoded signal, refer to `mu-law algorithm <https://en.wikipedia.org/wiki/M-law_algorithm>`_ .

    Args:
        quantization_channels (int, optional): Number of channels, which must be positive. Default: ``256``.

    Raises:
        TypeError: If `quantization_channels` is not of type int.
        ValueError: If `quantization_channels` is not a positive number.
        RuntimeError: If input tensor is not in shape of <..., time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 3, 4])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.MuLawDecoding()]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (3, 4) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([3, 4])  # 1 sample
        >>> output = audio.MuLawDecoding()(waveform)
        >>> print(output.shape, output.dtype)
        (3, 4) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_mu_law_coding
    def __init__(self, quantization_channels=256):
        super().__init__()
        self.quantization_channels = quantization_channels

    def parse(self):
        return cde.MuLawDecodingOperation(self.quantization_channels)


class MuLawEncoding(AudioTensorOperation):
    """
    Encode signal based on mu-law companding.

    Args:
        quantization_channels (int, optional): Number of channels, which must be positive. Default: ``256``.

    Raises:
        TypeError: If `quantization_channels` is not of type int.
        ValueError: If `quantization_channels` is not a positive number.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 3, 4])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.MuLawEncoding()]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (3, 4) int32
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([3, 4])  # 1 sample
        >>> output = audio.MuLawEncoding()(waveform)
        >>> print(output.shape, output.dtype)
        (3, 4) int32

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_mu_law_coding
    def __init__(self, quantization_channels=256):
        super().__init__()
        self.quantization_channels = quantization_channels

    def parse(self):
        return cde.MuLawEncodingOperation(self.quantization_channels)


class Overdrive(AudioTensorOperation):
    """
    Apply an overdrive effect to the audio waveform.

    Similar to `SoX <https://sourceforge.net/projects/sox/>`_ implementation.

    Args:
        gain (float, optional): Desired gain at the boost (or attenuation) in dB, in range of [0, 100].
            Default: ``20.0``.
        color (float, optional): Controls the amount of even harmonic content in the over-driven output,
            in range of [0, 100]. Default: ``20.0``.

    Raises:
        TypeError: If `gain` is not of type float.
        ValueError: If `gain` is not in range of [0, 100].
        TypeError: If `color` is not of type float.
        ValueError: If `color` is not in range of [0, 100].
        RuntimeError: If input tensor is not in shape of <..., time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 10])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.Overdrive()]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (10,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([10])  # 1 sample
        >>> output = audio.Overdrive()(waveform)
        >>> print(output.shape, output.dtype)
        (10,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_overdrive
    def __init__(self, gain=20.0, color=20.0):
        super().__init__()
        self.gain = gain
        self.color = color

    def parse(self):
        return cde.OverdriveOperation(self.gain, self.color)


class Phaser(AudioTensorOperation):
    """
    Apply a phasing effect to the audio.

    Similar to `SoX <https://sourceforge.net/projects/sox/>`_ implementation.

    Args:
        sample_rate (int): Sampling rate of the waveform, e.g. 44100 (Hz).
        gain_in (float, optional): Desired input gain at the boost (or attenuation) in dB,
            in range of [0.0, 1.0]. Default: ``0.4``.
        gain_out (float, optional): Desired output gain at the boost (or attenuation) in dB,
            in range of [0.0, 1e9]. Default: ``0.74``.
        delay_ms (float, optional): Desired delay in milliseconds, in range of [0.0, 5.0]. Default: ``3.0``.
        decay (float, optional): Desired decay relative to gain-in, in range of [0.0, 0.99]. Default: ``0.4``.
        mod_speed (float, optional): Modulation speed in Hz, in range of [0.1, 2.0]. Default: ``0.5``.
        sinusoidal (bool, optional): If ``True``, use sinusoidal modulation (preferable for multiple instruments).
            If ``False``, use triangular modulation (gives single instruments a sharper phasing effect).
            Default: ``True``.

    Raises:
        TypeError: If `sample_rate` is not of type int.
        TypeError: If `gain_in` is not of type float.
        ValueError: If `gain_in` is not in range of [0.0, 1.0].
        TypeError: If `gain_out` is not of type float.
        ValueError: If `gain_out` is not in range of [0.0, 1e9].
        TypeError: If `delay_ms` is not of type float.
        ValueError: If `delay_ms` is not in range of [0.0, 5.0].
        TypeError: If `decay` is not of type float.
        ValueError: If `decay` is not in range of [0.0, 0.99].
        TypeError: If `mod_speed` is not of type float.
        ValueError: If `mod_speed` is not in range of [0.1, 2.0].
        TypeError: If `sinusoidal` is not of type bool.
        RuntimeError: If input tensor is not in shape of <..., time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 12])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.Phaser(44100)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (12,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([12])  # 1 sample
        >>> output = audio.Phaser(44100)(waveform)
        >>> print(output.shape, output.dtype)
        (12,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_phaser
    def __init__(self, sample_rate, gain_in=0.4, gain_out=0.74, delay_ms=3.0, decay=0.4, mod_speed=0.5,
                 sinusoidal=True):
        super().__init__()
        self.decay = decay
        self.delay_ms = delay_ms
        self.gain_in = gain_in
        self.gain_out = gain_out
        self.mod_speed = mod_speed
        self.sample_rate = sample_rate
        self.sinusoidal = sinusoidal

    def parse(self):
        return cde.PhaserOperation(self.sample_rate, self.gain_in, self.gain_out,
                                   self.delay_ms, self.decay, self.mod_speed, self.sinusoidal)


class PhaseVocoder(AudioTensorOperation):
    """
    Given a STFT spectrogram, speed up in time without modifying pitch by a factor of rate.

    Args:
        rate (float): Speed-up factor.
        phase_advance (numpy.ndarray): Expected phase advance in each bin, in shape of (freq, 1).

    Raises:
        TypeError: If `rate` is not of type float.
        ValueError: If `rate` is not a positive number.
        TypeError: If `phase_advance` is not of type :class:`numpy.ndarray` .
        RuntimeError: If input tensor is not in shape of <..., freq, num_frame, complex=2>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 44, 10, 2])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.PhaseVocoder(rate=2, phase_advance=np.random.random([44, 1]))]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (44, 5, 2) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([44, 10, 2])  # 1 sample
        >>> output = audio.PhaseVocoder(rate=2, phase_advance=np.random.random([44, 1]))(waveform)
        >>> print(output.shape, output.dtype)
        (44, 5, 2) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_phase_vocoder
    def __init__(self, rate, phase_advance):
        super().__init__()
        self.rate = rate
        self.phase_advance = cde.Tensor(phase_advance)

    def parse(self):
        return cde.PhaseVocoderOperation(self.rate, self.phase_advance)


class PitchShift(AudioTensorOperation):
    """
    Shift the pitch of a waveform by `n_steps` steps.

    Args:
        sample_rate (int): Sampling rate of waveform (in Hz).
        n_steps (int): The steps to shift waveform.
        bins_per_octave (int, optional): The number of steps per octave. Default: ``12``.
        n_fft (int, optional): Size of FFT, creates `n_fft // 2 + 1` bins. Default: ``512``.
        win_length (int, optional): Window size. Default: ``None``, will be set to `n_fft` .
        hop_length (int, optional): Length of hop between STFT windows. Default: ``None``,
            will be set to `win_length // 4` .
        window (WindowType, optional): Window tensor that is applied/multiplied to each frame/window.
            Default: ``WindowType.HANN``.

    Raises:
        TypeError: If `sample_rate` is not of type int.
        TypeError: If `n_steps` is not of type int.
        TypeError: If `bins_per_octave` is not of type int.
        TypeError: If `n_fft` is not of type int.
        TypeError: If `win_length` is not of type int.
        TypeError: If `hop_length` is not of type int.
        TypeError: If `window` is not of type :class:`mindspore.dataset.audio.WindowType` .
        ValueError: If `sample_rate` is a negative number.
        ValueError: If `bins_per_octave` is 0.
        ValueError: If `n_fft` is a negative number.
        ValueError: If `win_length` is not positive.
        ValueError: If `hop_length` is not positive.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 8, 30])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.PitchShift(sample_rate=16000, n_steps=4)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (8, 30) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([8, 30])  # 1 sample
        >>> output = audio.PitchShift(sample_rate=16000, n_steps=4)(waveform)
        >>> print(output.shape, output.dtype)
        (8, 30) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_pitch_shift
    def __init__(self, sample_rate, n_steps, bins_per_octave=12, n_fft=512, win_length=None,
                 hop_length=None, window=WindowType.HANN):
        super().__init__()
        self.sample_rate = sample_rate
        self.n_steps = n_steps
        self.bins_per_octave = bins_per_octave
        self.n_fft = n_fft
        self.win_length = win_length if win_length is not None else n_fft
        self.hop_length = hop_length if hop_length is not None else self.win_length // 4
        self.window = window

    def parse(self):
        return cde.PitchShiftOperation(self.sample_rate, self.n_steps, self.bins_per_octave, self.n_fft,
                                       self.win_length, self.hop_length, DE_C_WINDOW_TYPE.get(self.window))


DE_C_RESAMPLE_METHOD = {ResampleMethod.SINC_INTERPOLATION: cde.ResampleMethod.DE_RESAMPLE_SINC_INTERPOLATION,
                        ResampleMethod.KAISER_WINDOW: cde.ResampleMethod.DE_RESAMPLE_KAISER_WINDOW}


class Resample(AudioTensorOperation):
    """
    Resample a signal from one frequency to another. A resample method can be given.

    Args:
        orig_freq (float, optional): The original frequency of the signal, must be positive. Default: ``16000``.
        new_freq (float, optional): The desired frequency, must be positive. Default: ``16000``.
        resample_method (ResampleMethod, optional): The resample method to use, can be
            ``ResampleMethod.SINC_INTERPOLATION`` or ``ResampleMethod.KAISER_WINDOW``.
            Default: ``ResampleMethod.SINC_INTERPOLATION``.
        lowpass_filter_width (int, optional): Controls the sharpness of the filter, more means sharper but less
            efficient, must be positive. Default: ``6``.
        rolloff (float, optional): The roll-off frequency of the filter, as a fraction of the Nyquist. Lower values
            reduce anti-aliasing, but also reduce some of the highest frequencies, in range of (0, 1].
            Default: ``0.99``.
        beta (float, optional): The shape parameter used for kaiser window. Default: ``None``,
            will use ``14.769656459379492``.

    Raises:
        TypeError: If `orig_freq` is not of type float.
        ValueError: If `orig_freq` is not a positive number.
        TypeError: If `new_freq` is not of type float.
        ValueError: If `new_freq` is not a positive number.
        TypeError: If `resample_method` is not of type :class:`mindspore.dataset.audio.ResampleMethod` .
        TypeError: If `lowpass_filter_width` is not of type int.
        ValueError: If `lowpass_filter_width` is not a positive number.
        TypeError: If `rolloff` is not of type float.
        ValueError: If `rolloff` is not in range of (0, 1].
        RuntimeError: If input tensor is not in shape of <..., time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 16, 30])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.Resample(orig_freq=48000, new_freq=16000,
        ...                              resample_method=audio.ResampleMethod.SINC_INTERPOLATION,
        ...                              lowpass_filter_width=6, rolloff=0.99, beta=None)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (16, 10) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([16, 30])  # 1 sample
        >>> output = audio.Resample(orig_freq=48000, new_freq=16000,
        ...                         resample_method=audio.ResampleMethod.SINC_INTERPOLATION,
        ...                         lowpass_filter_width=6, rolloff=0.99, beta=None)(waveform)
        >>> print(output.shape, output.dtype)
        (16, 10) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_resample
    def __init__(self, orig_freq=16000, new_freq=16000, resample_method=ResampleMethod.SINC_INTERPOLATION,
                 lowpass_filter_width=6, rolloff=0.99, beta=None):
        super().__init__()
        self.orig_freq = orig_freq
        self.new_freq = new_freq
        self.resample_method = resample_method
        self.lowpass_filter_width = lowpass_filter_width
        self.rolloff = rolloff
        kaiser_beta = 14.769656459379492
        self.beta = beta if beta is not None else kaiser_beta

    def parse(self):
        return cde.ResampleOperation(self.orig_freq, self.new_freq, DE_C_RESAMPLE_METHOD.get(self.resample_method),
                                     self.lowpass_filter_width, self.rolloff, self.beta)


class RiaaBiquad(AudioTensorOperation):
    """
    Apply RIAA vinyl playback equalization.

    Similar to `SoX <https://sourceforge.net/projects/sox/>`_ implementation.

    Args:
        sample_rate (int): sampling rate of the waveform, e.g. 44100 (Hz),
            can only be one of 44100, 48000, 88200, 96000.

    Raises:
        TypeError: If `sample_rate` is not of type int.
        ValueError: If `sample_rate` is not any of [44100, 48000, 88200, 96000].

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 24])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.RiaaBiquad(44100)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (24,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([24])  # 1 sample
        >>> output = audio.RiaaBiquad(44100)(waveform)
        >>> print(output.shape, output.dtype)
        (24,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_riaa_biquad
    def __init__(self, sample_rate):
        super().__init__()
        self.sample_rate = sample_rate

    def parse(self):
        return cde.RiaaBiquadOperation(self.sample_rate)


class SlidingWindowCmn(AudioTensorOperation):
    """
    Apply sliding-window cepstral mean (and optionally variance) normalization per utterance.

    Args:
        cmn_window (int, optional): Window in frames for running average CMN computation. Default: ``600``.
        min_cmn_window (int, optional): Minimum CMN window used at start of decoding (adds latency only at start).
            Only applicable if center is ``False``, ignored if center is ``True``. Default: ``100``.
        center (bool, optional): If ``True``, use a window centered on the current frame. If ``False``, window is
            to the left. Default: ``False``.
        norm_vars (bool, optional): If ``True``, normalize variance to one. Default: ``False``.

    Raises:
        TypeError: If `cmn_window` is not of type int.
        ValueError: If `cmn_window` is a negative number.
        TypeError: If `min_cmn_window` is not of type int.
        ValueError: If `min_cmn_window` is a negative number.
        TypeError: If `center` is not of type bool.
        TypeError: If `norm_vars` is not of type bool.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 16, 3])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.SlidingWindowCmn()]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (16, 3) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([16, 3])  # 1 sample
        >>> output = audio.SlidingWindowCmn()(waveform)
        >>> print(output.shape, output.dtype)
        (16, 3) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_sliding_window_cmn
    def __init__(self, cmn_window=600, min_cmn_window=100, center=False, norm_vars=False):
        super().__init__()
        self.cmn_window = cmn_window
        self.min_cmn_window = min_cmn_window
        self.center = center
        self.norm_vars = norm_vars

    def parse(self):
        return cde.SlidingWindowCmnOperation(self.cmn_window, self.min_cmn_window, self.center, self.norm_vars)


DE_C_WINDOW_TYPE = {WindowType.BARTLETT: cde.WindowType.DE_WINDOW_TYPE_BARTLETT,
                    WindowType.BLACKMAN: cde.WindowType.DE_WINDOW_TYPE_BLACKMAN,
                    WindowType.HAMMING: cde.WindowType.DE_WINDOW_TYPE_HAMMING,
                    WindowType.HANN: cde.WindowType.DE_WINDOW_TYPE_HANN,
                    WindowType.KAISER: cde.WindowType.DE_WINDOW_TYPE_KAISER}


class SpectralCentroid(TensorOperation):
    """
    Compute the spectral centroid for each channel along the time axis.

    Args:
        sample_rate (int): Sampling rate of audio signal, e.g. ``44100`` (Hz).
        n_fft (int, optional): Size of FFT, creates `n_fft // 2 + 1` bins. Default: ``400``.
        win_length (int, optional): Window size. Default: ``None``, will use `n_fft` .
        hop_length (int, optional): Length of hop between STFT windows. Default: ``None``, will use `win_length // 2` .
        pad (int, optional): Two sided padding of signal. Default: ``0``.
        window (WindowType, optional): Window function that is applied/multiplied to each frame/window,
            can be ``WindowType.BARTLETT``, ``WindowType.BLACKMAN``, ``WindowType.HAMMING``, ``WindowType.HANN``
            or ``WindowType.KAISER``. Default: ``WindowType.HANN``.

    Raises:
        TypeError: If `sample_rate` is not of type int.
        ValueError: If `sample_rate` is a negative number.
        TypeError: If `n_fft` is not of type int.
        ValueError: If `n_fft` is not a positive number.
        TypeError: If `win_length` is not of type int.
        ValueError: If `win_length` is not a positive number.
        ValueError: If `win_length` is greater than `n_fft` .
        TypeError: If `hop_length` is not of type int.
        ValueError: If `hop_length` is not a positive number.
        TypeError: If `pad` is not of type int.
        ValueError: If `pad` is a negative number.
        TypeError: If `window` is not of type :class:`mindspore.dataset.audio.WindowType` .
        RuntimeError: If input tensor is not in shape of <..., time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 10, 20])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.SpectralCentroid(44100)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (10, 1, 1) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([10, 20])  # 1 sample
        >>> output = audio.SpectralCentroid(44100)(waveform)
        >>> print(output.shape, output.dtype)
        (10, 1, 1) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_spectral_centroid
    def __init__(self, sample_rate, n_fft=400, win_length=None, hop_length=None, pad=0, window=WindowType.HANN):
        super().__init__()
        self.sample_rate = sample_rate
        self.pad = pad
        self.window = window
        self.n_fft = n_fft
        self.win_length = win_length if win_length else n_fft
        self.hop_length = hop_length if hop_length else self.win_length // 2

    def parse(self):
        return cde.SpectralCentroidOperation(self.sample_rate, self.n_fft, self.win_length, self.hop_length,
                                             self.pad, DE_C_WINDOW_TYPE.get(self.window))


class Spectrogram(TensorOperation):
    """
    Create a spectrogram from an audio signal.

    Args:
        n_fft (int, optional): Size of FFT, creates `n_fft // 2 + 1` bins. Default: ``400``.
        win_length (int, optional): Window size. Default: ``None``, will use `n_fft` .
        hop_length (int, optional): Length of hop between STFT windows. Default: ``None``, will use `win_length // 2` .
        pad (int, optional): Two sided padding of signal. Default: ``0``.
        window (WindowType, optional): Window function that is applied/multiplied to each frame/window,
            can be ``WindowType.BARTLETT``, ``WindowType.BLACKMAN``, ``WindowType.HAMMING``, ``WindowType.HANN``
            or ``WindowType.KAISER``. Currently, Kaiser window is not supported on macOS. Default: ``WindowType.HANN``.
        power (float, optional): Exponent for the magnitude spectrogram, must be non negative,
            e.g., ``1`` for energy, ``2`` for power, etc. Default: ``2.0``.
        normalized (bool, optional): Whether to normalize by magnitude after stft. Default: ``False``.
        center (bool, optional): Whether to pad waveform on both sides. Default: ``True``.
        pad_mode (BorderType, optional): Controls the padding method used when `center` is ``True``,
            can be ``BorderType.REFLECT``, ``BorderType.CONSTANT``, ``BorderType.EDGE`` or ``BorderType.SYMMETRIC``.
            Default: ``BorderType.REFLECT``.
        onesided (bool, optional): Controls whether to return half of results to avoid redundancy. Default: ``True``.

    Raises:
        TypeError: If `n_fft` is not of type int.
        ValueError: If `n_fft` is not a positive number.
        TypeError: If `win_length` is not of type int.
        ValueError: If `win_length` is not a positive number.
        ValueError: If `win_length` is greater than `n_fft` .
        TypeError: If `hop_length` is not of type int.
        ValueError: If `hop_length` is not a positive number.
        TypeError: If `pad` is not of type int.
        ValueError: If `pad` is a negative number.
        TypeError: If `window` is not of type :class:`mindspore.dataset.audio.WindowType` .
        TypeError: If `power` is not of type float.
        ValueError: If `power` is a negative number.
        TypeError: If `normalized` is not of type bool.
        TypeError: If `center` is not of type bool.
        TypeError: If `pad_mode` is not of type :class:`mindspore.dataset.audio.BorderType` .
        TypeError: If `onesided` is not of type bool.
        RuntimeError: If input tensor is not in shape of <..., time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 10, 20])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.Spectrogram()]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (10, 201, 1) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([10, 20])  # 1 sample
        >>> output = audio.Spectrogram()(waveform)
        >>> print(output.shape, output.dtype)
        (10, 201, 1) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_spectrogram
    def __init__(self, n_fft=400, win_length=None, hop_length=None, pad=0, window=WindowType.HANN, power=2.0,
                 normalized=False, center=True, pad_mode=BorderType.REFLECT, onesided=True):
        super().__init__()
        self.n_fft = n_fft
        self.win_length = win_length if win_length else n_fft
        self.hop_length = hop_length if hop_length else self.win_length // 2
        self.pad = pad
        self.window = window
        self.power = power
        self.normalized = normalized
        self.center = center
        self.pad_mode = pad_mode
        self.onesided = onesided

    def parse(self):
        return cde.SpectrogramOperation(self.n_fft, self.win_length, self.hop_length, self.pad,
                                        DE_C_WINDOW_TYPE.get(self.window), self.power, self.normalized,
                                        self.center, DE_C_BORDER_TYPE.get(self.pad_mode), self.onesided)


class TimeMasking(AudioTensorOperation):
    """
    Apply masking to a spectrogram in the time domain.

    Note:
        The shape of the audio waveform to be processed needs to be <..., freq, time>.

    Args:
        iid_masks (bool, optional): Whether to apply different masks to each example/channel. Default: ``False``.
        time_mask_param (int, optional): When `iid_masks` is ``True``, length of the mask will be uniformly sampled
            from [0, time_mask_param]; When `iid_masks` is ``False``, directly use it as length of the mask.
            The value should be in range of [0, time_length], where `time_length` is the length of audio waveform
            in time domain. Default: ``0``.
        mask_start (int, optional): Starting point to apply mask, only works when `iid_masks` is ``True``.
            The value should be in range of [0, time_length - time_mask_param], where `time_length` is
            the length of audio waveform in time domain. Default: ``0``.
        mask_value (float, optional): Value to assign to the masked columns. Default: ``0.0``.

    Raises:
        TypeError: If `iid_masks` is not of type bool.
        TypeError: If `time_mask_param` is not of type int.
        ValueError: If `time_mask_param` is greater than the length of audio waveform in time domain.
        TypeError: If `mask_start` is not of type int.
        ValueError: If `mask_start` a negative number.
        TypeError: If `mask_value` is not of type float.
        ValueError: If `mask_value` is a negative number.
        RuntimeError: If input tensor is not in shape of <..., freq, time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 16, 2])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.TimeMasking(time_mask_param=1)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (16, 2) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([16, 2])  # 1 sample
        >>> output = audio.TimeMasking(time_mask_param=1)(waveform)
        >>> print(output.shape, output.dtype)
        (16, 2) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_

    .. image:: time_masking_original.png

    .. image:: time_masking.png
    """

    @check_masking
    def __init__(self, iid_masks=False, time_mask_param=0, mask_start=0, mask_value=0.0):
        super().__init__()
        self.iid_masks = iid_masks
        self.time_mask_param = time_mask_param
        self.mask_start = mask_start
        self.mask_value = mask_value

    def parse(self):
        return cde.TimeMaskingOperation(self.iid_masks, self.time_mask_param, self.mask_start, self.mask_value)


class TimeStretch(AudioTensorOperation):
    """
    Stretch Short Time Fourier Transform (STFT) in time without modifying pitch for a given rate.

    Note:
        The shape of the audio waveform to be processed needs to be <..., freq, time, complex=2>.
        The first dimension represents the real part while the second represents the imaginary.

    Args:
        hop_length (int, optional): Length of hop between STFT windows, i.e. the number of samples
            between consecutive frames. Default: ``None``, will use `n_freq - 1` .
        n_freq (int, optional): Number of filter banks from STFT. Default: ``201``.
        fixed_rate (float, optional): Rate to speed up or slow down by. Default: ``None``, will keep
            the original rate.

    Raises:
        TypeError: If `hop_length` is not of type int.
        ValueError: If `hop_length` is not a positive number.
        TypeError: If `n_freq` is not of type int.
        ValueError: If `n_freq` is not a positive number.
        TypeError: If `fixed_rate` is not of type float.
        ValueError: If `fixed_rate` is not a positive number.
        RuntimeError: If input tensor is not in shape of <..., freq, num_frame, complex=2>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 16, 8, 2])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.TimeStretch()]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (1, 16, 8, 2) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([16, 8, 2])  # 1 sample
        >>> output = audio.TimeStretch()(waveform)
        >>> print(output.shape, output.dtype)
        (1, 16, 8, 2) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_

    .. image:: time_stretch_rate1.5.png

    .. image:: time_stretch_original.png

    .. image:: time_stretch_rate0.8.png
    """

    @check_time_stretch
    def __init__(self, hop_length=None, n_freq=201, fixed_rate=None):
        super().__init__()
        self.n_freq = n_freq
        self.fixed_rate = fixed_rate

        n_fft = (n_freq - 1) * 2
        self.hop_length = hop_length if hop_length is not None else n_fft // 2
        self.fixed_rate = fixed_rate if fixed_rate is not None else 1

    def parse(self):
        return cde.TimeStretchOperation(self.hop_length, self.n_freq, self.fixed_rate)


class TrebleBiquad(AudioTensorOperation):
    """
    Design a treble tone-control effect.

    Similar to `SoX <https://sourceforge.net/projects/sox/>`_ implementation.

    Args:
        sample_rate (int): Sampling rate (in Hz), which can't be zero.
        gain (float): Desired gain at the boost (or attenuation) in dB.
        central_freq (float, optional): Central frequency (in Hz). Default: ``3000``.
        Q (float, optional): `Quality factor <https://en.wikipedia.org/wiki/Q_factor>`_ ,
            in range of (0, 1]. Default: ``0.707``.

    Raises:
        TypeError: If `sample_rate` is not of type int.
        ValueError: If `sample_rate` is 0.
        TypeError: If `gain` is not of type float.
        TypeError: If `central_freq` is not of type float.
        TypeError: If `Q` is not of type float.
        ValueError: If `Q` is not in range of (0, 1].
        RuntimeError: If input tensor is not in shape of <..., time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 20])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.TrebleBiquad(44100, 200.0)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (20,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([20])  # 1 sample
        >>> output = audio.TrebleBiquad(44100, 200.0)(waveform)
        >>> print(output.shape, output.dtype)
        (20,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_treble_biquad
    def __init__(self, sample_rate, gain, central_freq=3000, Q=0.707):
        super().__init__()
        self.sample_rate = sample_rate
        self.gain = gain
        self.central_freq = central_freq
        self.quality_factor = Q

    def parse(self):
        return cde.TrebleBiquadOperation(self.sample_rate, self.gain, self.central_freq, self.quality_factor)


class Vad(AudioTensorOperation):
    """
    Voice activity detector.

    Attempt to trim silence and quiet background sounds from the ends of recordings of speech.

    Similar to `SoX <https://sourceforge.net/projects/sox/>`_ implementation.

    Args:
        sample_rate (int): Sampling rate of audio signal.
        trigger_level (float, optional): The measurement level used to trigger activity detection. Default: ``7.0``.
        trigger_time (float, optional): The time constant (in seconds) used to help ignore short bursts of
            sounds. Default: ``0.25``.
        search_time (float, optional): The amount of audio (in seconds) to search for quieter/shorter bursts of audio
            to include prior to the detected trigger point. Default: ``1.0``.
        allowed_gap (float, optional): The allowed gap (in seconds) between quieter/shorter bursts of audio to include
            prior to the detected trigger point. Default: ``0.25``.
        pre_trigger_time (float, optional): The amount of audio (in seconds) to preserve before the trigger point and
            any found quieter/shorter bursts. Default: ``0.0``.
        boot_time (float, optional): The time for the initial noise estimate. Default: ``0.35``.
        noise_up_time (float, optional): Time constant used by the adaptive noise estimator for when the noise level is
            increasing. Default: ``0.1``.
        noise_down_time (float, optional): Time constant used by the adaptive noise estimator for when the noise level
            is decreasing. Default: ``0.01``.
        noise_reduction_amount (float, optional): Amount of noise reduction to use in the detection algorithm.
            Default: 1.35.
        measure_freq (float, optional): Frequency of the algorithm's processing/measurements. Default: ``20.0``.
        measure_duration (float, optional): The duration of measurement. Default: ``None``,
            will use twice the measurement period.
        measure_smooth_time (float, optional): Time constant used to smooth spectral measurements. Default: ``0.4``.
        hp_filter_freq (float, optional): The 'Brick-wall' frequency of high-pass filter applied at the input to the
            detector algorithm. Default: ``50.0``.
        lp_filter_freq (float, optional): The 'Brick-wall' frequency of low-pass filter applied at the input to the
            detector algorithm. Default: ``6000.0``.
        hp_lifter_freq (float, optional): The 'Brick-wall' frequency of high-pass lifter used in the
            detector algorithm. Default: ``150.0``.
        lp_lifter_freq (float, optional): The 'Brick-wall' frequency of low-pass lifter used in the
            detector algorithm. Default: ``2000.0``.

    Raises:
        TypeError: If `sample_rate` is not of type int.
        ValueError: If `sample_rate` is not a positive number.
        TypeError: If `trigger_level` is not of type float.
        TypeError: If `trigger_time` is not of type float.
        ValueError: If `trigger_time` is a negative number.
        TypeError: If `search_time` is not of type float.
        ValueError: If `search_time` is a negative number.
        TypeError: If `allowed_gap` is not of type float.
        ValueError: If `allowed_gap` is a negative number.
        TypeError: If `pre_trigger_time` is not of type float.
        ValueError: If `pre_trigger_time` is a negative number.
        TypeError: If `boot_time` is not of type float.
        ValueError: If `boot_time` is a negative number.
        TypeError: If `noise_up_time` is not of type float.
        ValueError: If `noise_up_time` is a negative number.
        TypeError: If `noise_down_time` is not of type float.
        ValueError: If `noise_down_time` is a negative number.
        ValueError: If `noise_up_time` is less than `noise_down_time` .
        TypeError: If `noise_reduction_amount` is not of type float.
        ValueError: If `noise_reduction_amount` is a negative number.
        TypeError: If `measure_freq` is not of type float.
        ValueError: If `measure_freq` is not a positive number.
        TypeError: If `measure_duration` is not of type float.
        ValueError: If `measure_duration` is a negative number.
        TypeError: If `measure_smooth_time` is not of type float.
        ValueError: If `measure_smooth_time` is a negative number.
        TypeError: If `hp_filter_freq` is not of type float.
        ValueError: If `hp_filter_freq` is not a positive number.
        TypeError: If `lp_filter_freq` is not of type float.
        ValueError: If `lp_filter_freq` is not a positive number.
        TypeError: If `hp_lifter_freq` is not of type float.
        ValueError: If `hp_lifter_freq` is not a positive number.
        TypeError: If `lp_lifter_freq` is not of type float.
        ValueError: If `lp_lifter_freq` is not a positive number.
        RuntimeError: If input tensor is not in shape of <..., time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 1000])  # 5 samples
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.Vad(sample_rate=600)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (660,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([1000])  # 1 sample
        >>> output = audio.Vad(sample_rate=600)(waveform)
        >>> print(output.shape, output.dtype)
        (660,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_vad
    def __init__(self, sample_rate, trigger_level=7.0, trigger_time=0.25, search_time=1.0, allowed_gap=0.25,
                 pre_trigger_time=0.0, boot_time=0.35, noise_up_time=0.1, noise_down_time=0.01,
                 noise_reduction_amount=1.35, measure_freq=20.0, measure_duration=None, measure_smooth_time=0.4,
                 hp_filter_freq=50.0, lp_filter_freq=6000.0, hp_lifter_freq=150.0, lp_lifter_freq=2000.0):
        super().__init__()
        self.sample_rate = sample_rate
        self.trigger_level = trigger_level
        self.trigger_time = trigger_time
        self.search_time = search_time
        self.allowed_gap = allowed_gap
        self.pre_trigger_time = pre_trigger_time
        self.boot_time = boot_time
        self.noise_up_time = noise_up_time
        self.noise_down_time = noise_down_time
        self.noise_reduction_amount = noise_reduction_amount
        self.measure_freq = measure_freq
        self.measure_duration = measure_duration if measure_duration else 2.0 / measure_freq
        self.measure_smooth_time = measure_smooth_time
        self.hp_filter_freq = hp_filter_freq
        self.lp_filter_freq = lp_filter_freq
        self.hp_lifter_freq = hp_lifter_freq
        self.lp_lifter_freq = lp_lifter_freq

    def parse(self):
        return cde.VadOperation(self.sample_rate, self.trigger_level, self.trigger_time, self.search_time,
                                self.allowed_gap, self.pre_trigger_time, self.boot_time, self.noise_up_time,
                                self.noise_down_time, self.noise_reduction_amount, self.measure_freq,
                                self.measure_duration, self.measure_smooth_time, self.hp_filter_freq,
                                self.lp_filter_freq, self.hp_lifter_freq, self.lp_lifter_freq)


DE_C_GAIN_TYPE = {GainType.AMPLITUDE: cde.GainType.DE_GAIN_TYPE_AMPLITUDE,
                  GainType.POWER: cde.GainType.DE_GAIN_TYPE_POWER,
                  GainType.DB: cde.GainType.DE_GAIN_TYPE_DB}


class Vol(AudioTensorOperation):
    """
    Adjust volume of waveform.

    Args:
        gain (float): Gain at the boost (or attenuation).
            If `gain_type` is ``GainType.AMPLITUDE``, it is a non negative amplitude ratio.
            If `gain_type` is ``GainType.POWER``, it is a power (voltage squared).
            If `gain_type` is ``GainType.DB``, it is in decibels.
        gain_type (GainType, optional): Type of gain, can be ``GainType.AMPLITUDE``, ``GainType.POWER``
            or ``GainType.DB``. Default: ``GainType.AMPLITUDE``.

    Raises:
        TypeError: If `gain` is not of type float.
        TypeError: If `gain_type` is not of type :class:`mindspore.dataset.audio.GainType` .
        ValueError: If `gain` is a negative number when `gain_type` is ``GainType.AMPLITUDE``.
        ValueError: If `gain` is not a positive number when `gain_type` is ``GainType.POWER``.
        RuntimeError: If input tensor is not in shape of <..., time>.

    Supported Platforms:
        ``CPU``

    Examples:
        >>> import numpy as np
        >>> import mindspore.dataset as ds
        >>> import mindspore.dataset.audio as audio
        >>>
        >>> # Use the transform in dataset pipeline mode
        >>> waveform = np.random.random([5, 30])  # 5 sample
        >>> numpy_slices_dataset = ds.NumpySlicesDataset(data=waveform, column_names=["audio"])
        >>> transforms = [audio.Vol(gain=10, gain_type=audio.GainType.DB)]
        >>> numpy_slices_dataset = numpy_slices_dataset.map(operations=transforms, input_columns=["audio"])
        >>> for item in numpy_slices_dataset.create_dict_iterator(num_epochs=1, output_numpy=True):
        ...     print(item["audio"].shape, item["audio"].dtype)
        ...     break
        (30,) float64
        >>>
        >>> # Use the transform in eager mode
        >>> waveform = np.random.random([30])  # 1 sample
        >>> output = audio.Vol(gain=10, gain_type=audio.GainType.DB)(waveform)
        >>> print(output.shape, output.dtype)
        (30,) float64

    Tutorial Examples:
        - `Illustration of audio transforms
          <https://www.mindspore.cn/docs/en/master/api_python/samples/dataset/audio_gallery.html>`_
    """

    @check_vol
    def __init__(self, gain, gain_type=GainType.AMPLITUDE):
        super().__init__()
        self.gain = gain
        self.gain_type = gain_type

    def parse(self):
        return cde.VolOperation(self.gain, DE_C_GAIN_TYPE.get(self.gain_type))
