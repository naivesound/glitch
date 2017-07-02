package com.naivesound.glitch;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.util.Log;
import android.view.inputmethod.EditorInfo;
import android.view.View;
import android.widget.Adapter;
import android.widget.LinearLayout;

import java.io.*;
import java.util.*;

import static trikita.anvil.DSL.*;

import trikita.anvil.RenderableAdapter;
import trikita.anvil.RenderableView;

public class MainActivity extends Activity {

    private final static String TAG = "Glitch";
    private final static int REQUEST_PERMISSIONS_CODE = 1001;

    /* Prefer 44.1k to 48k, otherwise look for the highest supported rate */
    public final static int[] SAMPLE_RATES = {22050, 44100, 48000, 32000, 24000,
                                              22050, 16000, 12000, 11025, 8000};

    /* PCM 16bit is guaranteed to be supported */
    public final static int[] SAMPLE_FORMAT = {
        AudioFormat.ENCODING_PCM_16BIT,
    };

    /* Prefer mono, but fall back to stereo */
    public final static int[] SAMPLE_CHANNEL_OUT = {
        AudioFormat.CHANNEL_OUT_MONO, AudioFormat.CHANNEL_OUT_STEREO,
    };

    private Glitch mGlitch;
    private Thread mAudioThread;

    private Handler mHandler = new Handler();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (android.os.Build.VERSION.SDK_INT >=
            android.os.Build.VERSION_CODES.M) {
            if (checkSelfPermission(
                    Manifest.permission.READ_EXTERNAL_STORAGE) !=
                    PackageManager.PERMISSION_GRANTED ||
                checkSelfPermission(
                    Manifest.permission.WRITE_EXTERNAL_STORAGE) !=
                    PackageManager.PERMISSION_GRANTED) {

                Log.d(TAG, "Permissions are not granted. Request");

                requestPermissions(
                    new String[] {Manifest.permission.READ_EXTERNAL_STORAGE,
                                  Manifest.permission.WRITE_EXTERNAL_STORAGE},
                    REQUEST_PERMISSIONS_CODE);
                return;
            }
        }

        Log.d(TAG, "Permissions are already granted. Load UI");
        mGlitch = new Glitch(48000, 2048);
        setContentView(new MainView(this));
    }

    @Override
    protected void onDestroy() {
        mGlitch.dispose();
        super.onDestroy();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode,
                                           String[] permissions,
                                           int[] grantResults) {
        Log.d(TAG,
              "onRequestPermissionsResult(): requestCode = " + requestCode);
        if (requestCode == REQUEST_PERMISSIONS_CODE &&
            grantResults.length > 0 &&
            grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            Log.d(TAG, "Permissions are granted. Proceed");
            mGlitch = new Glitch();
            setContentView(new MainView(this));
        }
    }

    private AudioTrack findAudioTrack() {
        for (int rate : SAMPLE_RATES) {
            for (int fmt : SAMPLE_FORMAT) {
                for (int config : SAMPLE_CHANNEL_OUT) {
                    try {
                        Log.d(TAG, "Attempting rate " + rate + "Hz, bits: " +
                                       fmt + ", channel: " + config);
                        int bufferSize =
                            AudioTrack.getMinBufferSize(rate, config, fmt);
                        if (bufferSize != AudioTrack.ERROR_BAD_VALUE) {
                            AudioTrack track = new AudioTrack(
                                AudioManager.STREAM_MUSIC, rate, config, fmt,
                                bufferSize, AudioTrack.MODE_STREAM);
                            if (track.getState() ==
                                AudioTrack.STATE_INITIALIZED)
                                return track;
                        }
                    } catch (Exception e) {
                        Log.e(TAG, rate + "Exception, keep trying.", e);
                    }
                }
            }
        }
        return null;
    }

    // public Thread play() {
    // final AudioTrack track = findAudioTrack();
    //// if (track == null) {
    // return null;
    ////}
    // Thread playbackThread = new Thread() {
    // public void run() {
    // int minBufferSize = AudioTrack.getMinBufferSize(
    // track.getPresetRate(), track.getChannelConfiguration(),
    // track.getAudioFormat());
    // Glitch.setPresetRate(track.getPresetRate());
    // boolean stereo = (track.getChannelConfiguration() ==
    // AudioFormat.CHANNEL_OUT_STEREO);
    // int numFrames = minBufferSize / 2;
    // numFrames = numFrames * 2;
    // short[] buffer = new short[stereo ? numFrames * 2 : numFrames];
    // Log.d("Glitch",
    //"playback started, buffer = " + minBufferSize + " " +
    //(1000 * minBufferSize / track.getPresetRate()) +
    //"ms");
    // track.play();
    // while (!Thread.interrupted()) {
    // long start = System.currentTimeMillis();
    // mGlitch.fill(buffer, numFrames, stereo);
    // Log.d("Glitch", "fill(): " +
    //(System.currentTimeMillis() - start) +
    //"ms");
    // int r = track.write(buffer, 0, numFrames);
    // if (r != numFrames) {
    // Log.d("Glitch", "write(): " + r + " " + numFrames);
    // if (r < 0) {
    // break;
    //}
    //}
    //}
    // track.stop();
    // track.release();
    //}
    //};
    // playbackThread.start();
    // return playbackThread;
    //}

    public class MainView extends RenderableView {

        private final static String SAMPLE_DIR = "glitch-presets";
        // sync edited preset code with its original file in 1.5 sec after end
        // of active editing
        private final static long SYNC_SAMPLE_CODE_DELAY = 1500;

        private List<String> mPresets = new ArrayList<>();
        private String mPresetDirPath;
        private String mPresetCode = "";
        private int mSelectedPreset;

        public MainView(Context context) {
            super(context);
            fetchPresetList();
        }

        private Adapter mPresetAdapter =
            RenderableAdapter.withItems(mPresets, (index, item) -> {
                textView(() -> {
                    size(FILL, dip(48));
                    padding(dip(5), 0);
                    gravity(LEFT | CENTER_VERTICAL);
                    singleLine(true);
                    text(item);
                });
            });

        @Override
        public void view() {
            if (mPresetCode.length() == 0 && mPresets.size() > 0) {
                loadPreset(mPresets.get(mSelectedPreset));
            }
            linearLayout(() -> {
                size(FILL, FILL);
                orientation(LinearLayout.VERTICAL);
                padding(dip(5));

                spinner(() -> {
                    init(() -> onItemSelected((a, v, pos, id) -> {
                             loadPreset(mPresets.get(pos));
                             mSelectedPreset = pos;
                         }));
                    size(FILL, WRAP);
                    margin(0, 0, 0, dip(10));
                    adapter(mPresetAdapter);
                });

                editText(() -> {
                    size(FILL, 0);
                    weight(1f);
                    singleLine(false);
                    imeOptions(EditorInfo.IME_FLAG_NO_ENTER_ACTION);
                    gravity(TOP | LEFT);
                    text(mPresetCode);
                    onTextChanged(s -> {
                        mHandler.removeCallbacks(mSyncPresetRunnable);
                        if (!mGlitch.compile(s.toString())) {
                            Log.d(TAG, "syntax: error");
                        } else {
                            mPresetCode = s.toString();
                            Log.d(TAG, "syntax: ok");
                            mHandler.postDelayed(mSyncPresetRunnable,
                                                 SYNC_SAMPLE_CODE_DELAY);
                        }
                    });
                });
            });
        }

        private Runnable mSyncPresetRunnable = () -> {
            Log.d(TAG, "updating preset code in file " +
                           mPresets.get(mSelectedPreset));
            File f = new File(mPresetDirPath, mPresets.get(mSelectedPreset));
            FileWriter writer = null;
            try {
                writer = new FileWriter(f, false);
                writer.write(mPresetCode + "\n");
                writer.close();
            } catch (IOException e) {
                e.printStackTrace();
            } finally {
                if (writer != null) {
                    try {
                        writer.close();
                    } catch (IOException ex) {
                    }
                }
            }
        };

        private void fetchPresetList() {
            File root = Environment.getExternalStorageDirectory();

            if (!Environment.getExternalStorageState(root).equals(
                    Environment.MEDIA_MOUNTED)) {
                throw new RuntimeException("Media storage is unmounted!");
            }

            File presetDir = new File(root, SAMPLE_DIR);
            presetDir.mkdir();
            mPresetDirPath = presetDir.getAbsolutePath();

            File[] files = presetDir.listFiles();
            if (files == null) {
                throw new RuntimeException("Preset directory is empty");
            }
            for (File f : files) {
                if (!f.isDirectory()) {
                    mPresets.add(f.getName());
                }
            }
        }

        private void loadPreset(String name) {
            mPresetCode = getPresetCode(name);
            mGlitch.compile(mPresetCode);
        }

        private String getPresetCode(String name) {
            File f = new File(mPresetDirPath, name);
            if (f.exists()) {
                try {
                    return new Scanner(f).useDelimiter("\\Z").next();
                } catch (FileNotFoundException e) {
                    e.printStackTrace();
                    return "# Failed to load preset code";
                }
            } else {
                return "Preset file not found";
            }
        }
    }

    private class GlitchView extends RenderableView {

        private String mScript =
            "bpm = 120\n"
            + "\n"
            +
            "$(organ, lpf($2*(saw(hz($1))+saw(hz($1+12))), 5*env($2+$3, 0.2, 2.8)))\n"
            + "$(keyenv, ($1, env($2, 0.001, 1.4)))\n"
            + "\n"
            +
            "bassriff = seq(bpm*2, G#2, (2,C#2), C#2, (2,C#3), (2,B2), G#2, (7,C#2))\n"
            +
            "bassriff = lpf(saw(hz(bassriff)), hz(bassriff)*0.9) * lpf(r(), 2*hz(bassriff))\n"
            + "bassriff = env(bassriff, 0.01, 2.5)\n"
            + "\n"
            + "#live = each((i,v)\n"
            + "#organ(i, v, 600)\n"
            + "#keyenv(k0, v0)\n"
            + "#keyenv(k1, v1)\n"
            + "#keyenv(k2, v2)\n"
            + "#keyenv(k3, v3)\n"
            + "#keyenv(k4, v4)\n"
            + "#keyenv(k5, v5)\n"
            + "#)\n"
            + "\n"
            + "#pad = y > 0 && fm(hz(G#4+x), 2, 3)-tri(hz(C#4+x)) || 0\n"
            + "#pad = lpf(pad, 500) * lpf(r(), 400) * (sin(1)/2 + 1) / 4\n"
            + "#pad = delay(pad, 0.125, 0.9, 0.9)\n"
            + "\n"
            + "drumvol = 1\n"
            + "((t / 8000) * 2 < 32) && (bassriff = 0)\n"
            + "((t / 8000) * 2 < 8) && (drumvol = 0)\n"
            + "\n"
            + "mix(\n"
            + "	bassriff\n"
            + "	tr808(BD, seq(bpm*2, 1, 1, 0, 0))\n"
            + "	tr808(SD, seq(bpm, 0, 1))\n"
            + "	drumvol * tr808(HH, seq(bpm*2, r(), r()/2))\n"
            + ")\n"
            + "";

        public GlitchView(Context context) {
            super(context);
            // mScript = "sin(440)";
            mGlitch.compile(mScript);
        }

        @Override
        public void view() {
            o(linearLayout(), orientation(LinearLayout.VERTICAL),

              o(editText(), size(FILL, 0), weight(1f), singleLine(false),
                text(mScript), imeOptions(EditorInfo.IME_FLAG_NO_ENTER_ACTION),

                onTextChanged(new SimpleTextWatcher() {
                    public void onTextChanged(CharSequence s) {
                        if (!mGlitch.compile(s.toString())) {
                            Log.d(TAG, "syntax: error");
                        } else {
                            mScript = s.toString();
                            Log.d(TAG, "syntax: ok");
                        }
                    }
                })),

              o(button(), size(FILL, WRAP), weight(0),
                onClick(new OnClickListener() {
                    public void onClick(View v) {
                        if (mAudioThread == null) {
                            Log.d(TAG, "Start playback");
                            // mAudioThread = play();
                        } else {
                            Log.d(TAG, "Stop playback");
                            // mAudioThread.interrupt();
                            // try {
                            // mAudioThread.join();
                            //} catch (InterruptedException e) {
                            // Log.d("Glitch", "Interrupted: ", e);
                            //} finally {
                            // mAudioThread = null;
                            //}
                        }
                    }
                })));
        }
    }
}
