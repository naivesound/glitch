package com.naivesound.glitch;

import android.app.Activity;
import android.content.Context;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Bundle;
import android.text.InputType;
import android.util.Log;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethod;
import android.view.View;
import android.widget.EditText;
import android.widget.LinearLayout;

import static trikita.anvil.DSL.*;

import trikita.anvil.RenderableView;

public class MainActivity extends Activity {

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

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mGlitch = new Glitch();
        setContentView(new GlitchView(this));
    }

    @Override
    protected void onDestroy() {
        mGlitch.dispose();
        super.onDestroy();
    }

    private AudioTrack findAudioTrack() {
        for (int rate : SAMPLE_RATES) {
            for (int fmt : SAMPLE_FORMAT) {
                for (int config : SAMPLE_CHANNEL_OUT) {
                    try {
                        Log.d("Glitch", "Attempting rate " + rate +
                                            "Hz, bits: " + fmt + ", channel: " +
                                            config);
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
                        Log.e("Glitch", rate + "Exception, keep trying.", e);
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
    // track.getSampleRate(), track.getChannelConfiguration(),
    // track.getAudioFormat());
    // Glitch.setSampleRate(track.getSampleRate());
    // boolean stereo = (track.getChannelConfiguration() ==
    // AudioFormat.CHANNEL_OUT_STEREO);
    // int numFrames = minBufferSize / 2;
    // numFrames = numFrames * 2;
    // short[] buffer = new short[stereo ? numFrames * 2 : numFrames];
    // Log.d("Glitch",
    //"playback started, buffer = " + minBufferSize + " " +
    //(1000 * minBufferSize / track.getSampleRate()) +
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

    private class GlitchView extends RenderableView {
        private String mScript =
            "# MASSIVE ATTACK - ATLAS AIR\n"
            + "# TODO: make syntax\n"
            + "\n"
            + "bpm = 120\n"
            + "\n"
            +
            "$(organ, lpf($2*(saw(hz($1))+saw(hz($1+12))), env($2+$3, (0.2, 5), (0.3, 2), (0.5, 0.5), (2, 0.1))))\n"
            + "$(keyenv, ($1, env($2, (0.001, 1), (0.1, 0.2), (1.3, 0.1))))\n"
            + "\n"
            +
            "bassriff = seq(bpm*2, G#2, (2,C#2), C#2, (2,C#3), (2,B2), G#2, (7,C#2))\n"
            +
            "bassriff = lpf(saw(hz(bassriff)), hz(bassriff)*0.9) * lpf(r(), 2*hz(bassriff))\n"
            + "bassriff = env(bassriff, (0.5, 0.3), (2, 0.1))\n"
            + "\n"
            + "live = each((i,v)\n"
            + "organ(i, v, 600)\n"
            + "keyenv(k0, v0)\n"
            + "keyenv(k1, v1)\n"
            + "keyenv(k2, v2)\n"
            + "keyenv(k3, v3)\n"
            + "keyenv(k4, v4)\n"
            + "keyenv(k5, v5)\n"
            + ")\n"
            + "\n"
            + "pad = y > 0 && fm(hz(G#4+x), 2, 3)-tri(hz(C#4+x)) || 0\n"
            + "pad = lpf(pad, 500) * lpf(r(), 400) * (sin(1)/2 + 1) / 4\n"
            + "pad = delay(pad, 0.125, 0.9, 0.9)\n"
            + "\n"
            + "drumvol = 1\n"
            + "((t / 8000) * 2 < 32) && (bassriff = 0)\n"
            + "((t / 8000) * 2 < 8) && (drumvol = 0)\n"
            + "\n"
            + "mix(\n"
            + "	live*0.7\n"
            + "	pad\n"
            + "	bassriff\n"
            + "	tr808(BD, seq(bpm*2, 1, 1, 0, 0))\n"
            + "	tr808(SD, seq(bpm, 0, 1))\n"
            + "	drumvol * tr808(HH, seq(bpm*2, r(), r()/2))\n"
            +
            "	drumvol * tr808(OH, seq(bpm*2, 0, 0, r()/4+0.1, 0, 0, 0, 0, 0))\n"
            + ")\n"
            + "";
        public GlitchView(Context context) {
            super(context);
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
                            Log.d("Glitch", "syntax: error");
                        } else {
                            mScript = s.toString();
                            Log.d("Glitch", "syntax: ok");
                        }
                    }
                })),

              o(button(), size(FILL, WRAP), weight(0),
                onClick(new OnClickListener() {
                    public void onClick(View v) {
                        if (mAudioThread == null) {
                            Log.d("Glitch", "Start playback");
                            // mAudioThread = play();
                        } else {
                            Log.d("Glitch", "Stop playback");
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
