package com.airwavz.libatsc3.media;

import java.util.concurrent.locks.ReentrantLock;

public class ATSC3PlayerFlags {


    public static ReentrantLock ReentrantLock = new ReentrantLock();
    public static boolean ATSC3PlayerStartPlayback = false;
    public static boolean ATSC3PlayerStopPlayback = false;
    public static boolean FirstMfuBufferVideoKeyframeSent = false;
    public static long FirstMfuBuffer_presentation_time_us_mpu;



}
