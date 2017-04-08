package com.naivesound.glitch;

public class Glitch {
    /*
    float glitch_beat(struct glitch *g);
    void glitch_xy(struct glitch *g, float x, float y);
    void glitch_midi(struct glitch *g, unsigned char cmd, unsigned char a,
                     unsigned char b);
    */

    static { System.loadLibrary("glitch-lib"); }

    private long mRef;

    public Glitch() { mRef = create(); }
    public synchronized boolean compile(String s) { return compile(mRef, s); }
    public synchronized void fill(short[] buf, int frames, boolean stereo) {
        fill(mRef, buf, frames, stereo);
    }
    public synchronized void dispose() { destroy(mRef); }
    public synchronized void finalize() { dispose(); }

    public static native void setSampleRate(int sampleRate);

    private native long create();
    private native void destroy(long ref);
    private native boolean compile(long ref, String s);
    private native void fill(long ref, short buf[], int frames, boolean stereo);
}
