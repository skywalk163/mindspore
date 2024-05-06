/*
 * Copyright 2022 Huawei Technologies Co., Ltd
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

package com.mindspore.config;
import java.util.HashMap;
import java.util.ArrayList;
import java.util.logging.Logger;

/**
 * Configuration for ModelParallelRunner.
 *
 * @since v1.6
 */
public class RunnerConfig {
    private static final Logger LOGGER = Logger.getLogger(RunnerConfig.class.toString());
    static {
        MindsporeLite.init();
    }

    private long runnerConfigPtr;

    /**
     * Construct function.
     */
    public RunnerConfig() {
        this.runnerConfigPtr = 0L;
    }

    /**
     * Init RunnerConfig
     *
     * @return init status.
     */
    public boolean init() {
        this.runnerConfigPtr = createRunnerConfig();
        return this.runnerConfigPtr != 0L;
    }

    /**
     * Init RunnerConfig
     *
     * @param msContext MSContext Object.
     * @return init status.
     */
    public boolean init(MSContext msContext) {
        if (msContext == null) {
            return false;
        }
        this.runnerConfigPtr = createRunnerConfigWithContext(msContext.getMSContextPtr());
        return this.runnerConfigPtr != 0L;
    }

    /**
     * Set workers num
     *
     * @param workersNum The number of parallel models.
     */
    public void setWorkersNum(int workersNum) {
        setWorkersNum(runnerConfigPtr, workersNum);
    }

    /**
     * Set config info
     *
     * @param config_info The config info.
     */
    public void setConfigInfo(String section, HashMap<String, String> config) {
        setConfigInfo(runnerConfigPtr, section, config);
    }

    /**
     * Set config path
     *
     * @param config_path The config path.
     */
    public void setConfigPath(String config_path) {
        setConfigPath(runnerConfigPtr, config_path);
    }

    /**
     * @return Get config path
     */
    public String getConfigPath() {
        return getConfigPath(runnerConfigPtr);
    }


    /**
     * Get RunnerConfig pointer.
     *
     * @return RunnerConfig pointer.
     */
    public long getRunnerConfigPtr() {
        return runnerConfigPtr;
    }

    /**
     * Set device id list
     *
     * @param deviceIds The device id list.
     */
    public void setDeviceIds(ArrayList<Integer> deviceIds) {
        int len = deviceIds.size();
        if (len == 0) {
            LOGGER.severe("deviceIds is empty.");
            return;
        }
        int[] deviceIdsArray = new int[len];
        for (int i = 0; i < len; i++) {
            int deviceId = deviceIds.get(i);
            if (deviceId < 0) {
                LOGGER.severe("deviceIds contain nagetive number.");
                return;
            }
            deviceIdsArray[i] = deviceId;
        }
        setDeviceIds(runnerConfigPtr, deviceIdsArray);
    }

    /**
     * Get device id list.
     *
     * @return device id list.
     */
    public ArrayList<Integer> getDeviceIds() {
        return getDeviceIds(runnerConfigPtr);
    }

    /**
     * Fre RunnerConfig pointer.
     */
    public void free() {
        this.free(runnerConfigPtr);
        runnerConfigPtr = 0;
    }

    private native long createRunnerConfig();

    private native long createRunnerConfigWithContext(long msContextPtr);

    private native void setWorkersNum(long runnerConfigPtr, int workersNum);

    private native void setConfigInfo(long runnerConfigPtr, String section, HashMap<String, String> config);

    private native void setConfigPath(long runnerConfigPtr, String config_path);

    private native String getConfigPath(long runnerConfigPtr);

    private native void setDeviceIds(long runnerConfigPtr, int[] deviceIds);

    private native ArrayList<Integer> getDeviceIds(long runnerConfigPtr);

    private native boolean free(long runnerConfigPtr);
}
