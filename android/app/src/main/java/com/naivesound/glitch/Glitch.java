package com.naivesound.glitch;

public class Glitch {
    /*
    float glitch_beat(struct glitch *g);
    void glitch_midi(struct glitch *g, unsigned char cmd, unsigned char a,
                     unsigned char b);
    */

    static { System.loadLibrary("glitch-lib"); }

    private long mRef;

    public Glitch(int sampleRate, int frames) {
        mRef = create(sampleRate, frames);
    }
    public synchronized void glitch_xy(float x, float y) { xy(mRef, x, y); }
    public synchronized boolean compile(String s) { return compile(mRef, s); }
    public synchronized void dispose() { destroy(mRef); }
    public synchronized void finalize() { dispose(); }

    private native long create(int sampleRate, int frames);
    private native void destroy(long ref);
    private native boolean compile(long ref, String s);
    private native void xy(long ref, float x, float y);
}
