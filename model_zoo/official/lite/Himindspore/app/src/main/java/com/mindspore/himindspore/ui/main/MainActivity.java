/**
 * Copyright 2021 Huawei Technologies Co., Ltd
 * <p>
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * <p>
 * http://www.apache.org/licenses/LICENSE-2.0
 * <p>
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.mindspore.himindspore.ui.main;

import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.util.Log;
import android.widget.Toast;

import androidx.core.content.FileProvider;
import androidx.fragment.app.Fragment;
import androidx.viewpager.widget.ViewPager;

import com.mindspore.common.base.adapter.BasePagerAdapter;
import com.mindspore.customview.dialog.UpdateDialog;
import com.mindspore.customview.tablayout.CommonTabLayout;
import com.mindspore.customview.tablayout.listener.CustomTabEntity;
import com.mindspore.customview.tablayout.listener.OnTabSelectListener;
import com.mindspore.himindspore.R;
import com.mindspore.himindspore.base.BaseActivity;
import com.mindspore.himindspore.comment.FragmentFactory;
import com.mindspore.himindspore.net.FileDownLoadObserver;
import com.mindspore.himindspore.net.UpdateInfoBean;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

public class MainActivity extends BaseActivity<MainPresenter> implements MainContract.View {
    private static final String TAG = "MainActivity";

    private ViewPager mVpHome;
    private CommonTabLayout mCtlTable;

    private int now_version;
    private ProgressDialog progressDialog;

    @Override
    protected void init() {
        presenter = new MainPresenter(this);
        mVpHome = findViewById(R.id.vp_home);
        mCtlTable = findViewById(R.id.ctl_table);
        showPackaeInfo();
        initTabLayout();
        initViewPager();
        getUpdateInfo();
    }

    @Override
    public int getLayout() {
        return R.layout.activity_main;
    }


    private void showPackaeInfo() {
        try {
            PackageManager packageManager = this.getPackageManager();
            PackageInfo packageInfo = packageManager.getPackageInfo(this.getPackageName(), 0);
            now_version = packageInfo.versionCode;
        } catch (PackageManager.NameNotFoundException e) {
            e.printStackTrace();
        }
    }

    private void initTabLayout() {
        ArrayList<CustomTabEntity> mTabEntities = presenter.getTabEntity();
        mCtlTable.setTabData(mTabEntities);
        mCtlTable.setOnTabSelectListener(new OnTabSelectListener() {
            @Override
            public void onTabSelect(int position) {
                mVpHome.setCurrentItem(position);
            }

            @Override
            public void onTabReselect(int position) {

            }
        });
    }


    /**
     * 初始化ViewPager数据
     */
    private void initViewPager() {
        List<Fragment> fragments = new ArrayList<>();
        fragments.add(FragmentFactory.getInstance().getExperienceFragment());
        fragments.add(FragmentFactory.getInstance().getCollegeFragment());
        fragments.add(FragmentFactory.getInstance().getMeFragment());
        BasePagerAdapter adapter = new BasePagerAdapter(getSupportFragmentManager(), fragments);
        mVpHome.setAdapter(adapter);
        mVpHome.addOnPageChangeListener(new ViewPager.OnPageChangeListener() {
            @Override
            public void onPageScrolled(int position, float positionOffset, int positionOffsetPixels) {

            }

            @Override
            public void onPageSelected(int position) {
                if (position >= 0) {
                    mCtlTable.setCurrentTab(position);
                }
            }

            @Override
            public void onPageScrollStateChanged(int state) {
            }
        });
        mVpHome.setOffscreenPageLimit(3);
        mVpHome.setCurrentItem(0);
    }


    private void getUpdateInfo() {
        presenter.getUpdateInfo();
    }


    @Override
    public void showUpdateResult(UpdateInfoBean bean) {
        showUpdate(bean);
    }

    @Override
    public void showFail(String s) {

    }

    public void downSuccess() {
        if (progressDialog != null && progressDialog.isShowing()) {
            progressDialog.dismiss();
        }
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setIcon(android.R.drawable.ic_dialog_info);
        builder.setTitle(getResources().getString(R.string.app_download_success));
        builder.setMessage(getResources().getString(R.string.app_need_install));
        builder.setCancelable(false);
        builder.setPositiveButton(getResources().getString(R.string.confirm), (dialog, which) -> {
            Intent intent = new Intent(Intent.ACTION_VIEW);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                intent.setFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                Uri contentUri = FileProvider.getUriForFile(MainActivity.this, "com.mindspore.himindspore.fileprovider",
                        new File(getApkPath(), "HiMindSpore.apk"));
                intent.setDataAndType(contentUri, "application/vnd.android.package-archive");
            } else {
                intent.setDataAndType(Uri.fromFile(new File(getApkPath(), "HiMindSpore.apk")), "application/vnd.android.package-archive");
                intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            }
            startActivity(intent);
        });
        builder.setNegativeButton(getResources().getString(R.string.cancel), (dialog, which) -> {
        });
        builder.create().show();
    }


    public void showUpdate(final UpdateInfoBean updateInfo) {
        if (now_version != updateInfo.getVersionCode()) {
            UpdateDialog updateDialog= new UpdateDialog(this);
            updateDialog.setTitleString(getResources().getString(R.string.app_update_lastest) + updateInfo.getVersionName());
            updateDialog.setContentString(updateInfo.getMessage());
            updateDialog.setYesOnclickListener(() -> downFile());
            updateDialog.setNoOnclickListener(() -> updateDialog.dismiss());
            updateDialog.show();
        }
    }

    public void downFile() {
        progressDialog = new ProgressDialog(this);
        progressDialog.setProgressStyle(ProgressDialog.STYLE_HORIZONTAL);
        progressDialog.setTitle(getResources().getString(R.string.app_is_loading));
        progressDialog.setMessage(getResources().getString(R.string.app_wait));
        progressDialog.setProgressNumberFormat("%1d Mb/%2d Mb");
        progressDialog.setProgress(0);
        progressDialog.show();
        presenter.downloadApk(getApkPath(), "HiMindSpore.apk", new FileDownLoadObserver<File>() {
            @Override
            public void onDownLoadSuccess(File file) {
                downSuccess();
            }

            @Override
            public void onDownLoadFail(Throwable throwable) {
                Toast.makeText(MainActivity.this, getResources().getString(R.string.app_load_fail), Toast.LENGTH_LONG).show();
            }

            @Override
            public void onProgress(final int progress, final long total) {
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        progressDialog.setMax((int) total / 1024 / 1024);
                        progressDialog.setProgress(progress);
                    }
                });

            }
        });
        Log.d(TAG, "downFile: ");
    }

    public String getApkPath() {
        String directoryPath = "";
        if (Environment.MEDIA_MOUNTED.equals(Environment.getExternalStorageState())) {
            directoryPath = getExternalFilesDir("apk").getAbsolutePath();
        } else {
            directoryPath = getFilesDir() + File.separator + "apk";
        }
        File file = new File(directoryPath);
        if (!file.exists()) {
            file.mkdirs();
        }
        return directoryPath;
    }
}