mindspore.dataset.audio.TrebleBiquad
====================================

.. py:class:: mindspore.dataset.audio.TrebleBiquad(sample_rate, gain, central_freq=3000, Q=0.707)

    给音频波形施加高音音调控制效果。

    接口实现方式类似于 `SoX库 <http://sox.sourceforge.net/sox.html>`_ 。

    参数：
        - **sample_rate** (int) - 采样频率（单位：Hz），不能为0。
        - **gain** (float) - 期望提升（或衰减）的音频增益（单位：dB）。
        - **central_freq** (float, 可选) - 中心频率（单位：Hz）。默认值： ``3000`` 。
        - **Q** (float, 可选) - `品质因子 <https://zh.wikipedia.org/wiki/%E5%93%81%E8%B3%AA%E5%9B%A0%E5%AD%90>`_ ，能够反映带宽与采样频率和中心频率的关系，取值范围为(0, 1]，默认值： ``0.707`` 。

    异常：
        - **TypeError** - 当 `sample_rate` 的类型不为int。
        - **ValueError** - 当 `sample_rate` 的数值为0。
        - **TypeError** - 当 `gain` 的类型不为float。
        - **TypeError** - 当 `central_freq` 的类型不为float。
        - **TypeError** - 当 `Q` 的类型不为float。
        - **ValueError** - 当 `Q` 取值不在(0, 1]范围内。
        - **RuntimeError** - 当输入音频的shape不为<..., time>。

    教程样例：
        - `音频变换样例库
          <https://www.mindspore.cn/docs/zh-CN/master/api_python/samples/dataset/audio_gallery.html>`_
