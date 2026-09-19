package org.libsdl.app;


import android.content.Context;
import android.content.pm.ActivityInfo;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Build;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Display;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.WindowManager;


/**
    SDLSurface. This is what we draw on, so we need to know when it's created
    in order to do anything useful.

    Because of this, that's where we set up the SDL thread
*/
public class SDLSurface extends SurfaceView implements SurfaceHolder.Callback,
    View.OnKeyListener, View.OnTouchListener, SensorEventListener  {

    // Sensors
    protected SensorManager mSensorManager;
    protected Display mDisplay;

    // Keep track of the surface size to normalize touch events
    protected float mWidth, mHeight;

    // Is SurfaceView ready for rendering
    public boolean mIsSurfaceReady;

    /*
     * Touch screens do not report mouse secondary-button events.  Keep enough
     * state to turn a short, stationary two-finger tap into one instead.
     */
    private boolean mTwoFingerTapCandidate;
    private int mTwoFingerTapFirstPointerId;
    private int mTwoFingerTapSecondPointerId;
    private float mTwoFingerTapFirstX;
    private float mTwoFingerTapFirstY;
    private float mTwoFingerTapSecondX;
    private float mTwoFingerTapSecondY;
    private long mTwoFingerTapStartTime;
    private final float mTwoFingerTapSlop;
    private final float mTwoFingerZoomStep;
    private float mTwoFingerLastSpan;
    private static final long TWO_FINGER_DRAG_GRACE_PERIOD_MS = 100L;
    private boolean mTwoFingerDragActive;
    private boolean mPendingSingleTouch;
    private boolean mSuppressTouchSequence;
    private int mPendingTouchDeviceId;
    private int mPendingTouchPointerId;
    private float mPendingTouchX;
    private float mPendingTouchY;
    private float mPendingTouchPressure;
    private long mPendingTouchDownTime;

    // Startup
    public SDLSurface(Context context) {
        super(context);
        getHolder().addCallback(this);

        setFocusable(true);
        setFocusableInTouchMode(true);
        requestFocus();
        setOnKeyListener(this);
        setOnTouchListener(this);

        mDisplay = ((WindowManager)context.getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay();
        mSensorManager = (SensorManager)context.getSystemService(Context.SENSOR_SERVICE);

        setOnGenericMotionListener(SDLActivity.getMotionListener());

        // Some arbitrary defaults to avoid a potential division by zero
        mWidth = 1.0f;
        mHeight = 1.0f;

        mTwoFingerTapSlop = ViewConfiguration.get(context).getScaledTouchSlop() * 0.5f;
        // Require a deliberate change in finger separation for each wheel tick.
        // This filters natural jitter while retaining a responsive pinch gesture.
        mTwoFingerZoomStep = ViewConfiguration.get(context).getScaledTouchSlop() * 2.0f;

        mIsSurfaceReady = false;
    }

    public void handlePause() {
        enableSensor(Sensor.TYPE_ACCELEROMETER, false);
    }

    public void handleResume() {
        setFocusable(true);
        setFocusableInTouchMode(true);
        requestFocus();
        setOnKeyListener(this);
        setOnTouchListener(this);
        enableSensor(Sensor.TYPE_ACCELEROMETER, true);
    }

    public Surface getNativeSurface() {
        return getHolder().getSurface();
    }

    // Called when we have a valid drawing surface
    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Log.v("SDL", "surfaceCreated()");
        SDLActivity.onNativeSurfaceCreated();
    }

    // Called when we lose the surface
    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.v("SDL", "surfaceDestroyed()");

        // Transition to pause, if needed
        SDLActivity.mNextNativeState = SDLActivity.NativeState.PAUSED;
        SDLActivity.handleNativeState();

        mIsSurfaceReady = false;
        SDLActivity.onNativeSurfaceDestroyed();
    }

    // Called when the surface is resized
    @Override
    public void surfaceChanged(SurfaceHolder holder,
                               int format, int width, int height) {
        Log.v("SDL", "surfaceChanged()");

        if (SDLActivity.mSingleton == null) {
            return;
        }

        mWidth = width;
        mHeight = height;
        int nDeviceWidth = width;
        int nDeviceHeight = height;
        try
        {
            if (Build.VERSION.SDK_INT >= 17 /* Android 4.2 (JELLY_BEAN_MR1) */) {
                DisplayMetrics realMetrics = new DisplayMetrics();
                mDisplay.getRealMetrics( realMetrics );
                nDeviceWidth = realMetrics.widthPixels;
                nDeviceHeight = realMetrics.heightPixels;
            }
        } catch(Exception ignored) {
        }

        // Android can report the physical display in the orientation of the
        // launcher while the game Activity is being transitioned to its
        // requested orientation. SDL uses these dimensions for the desktop
        // display mode, so keep their orientation consistent with the game
        // surface before passing them to native code.
        int requestedOrientation = SDLActivity.mSingleton.getRequestedOrientation();
        boolean landscapeRequested = requestedOrientation == ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE ||
                requestedOrientation == ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE ||
                requestedOrientation == ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE;
        boolean portraitRequested = requestedOrientation == ActivityInfo.SCREEN_ORIENTATION_PORTRAIT ||
                requestedOrientation == ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT ||
                requestedOrientation == ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT;
        boolean swapDeviceOrientation = landscapeRequested && nDeviceWidth < nDeviceHeight ||
                portraitRequested && nDeviceWidth > nDeviceHeight;
        if (swapDeviceOrientation) {
            int orientedDeviceWidth = nDeviceWidth;
            nDeviceWidth = nDeviceHeight;
            nDeviceHeight = orientedDeviceWidth;
        }

        synchronized(SDLActivity.getContext()) {
            // In case we're waiting on a size change after going fullscreen, send a notification.
            SDLActivity.getContext().notifyAll();
        }

        Log.v("SDL", "Window size: " + width + "x" + height);
        Log.v("SDL", "Device size: " + nDeviceWidth + "x" + nDeviceHeight);
        SDLActivity.nativeSetScreenResolution(width, height, nDeviceWidth, nDeviceHeight, mDisplay.getRefreshRate());
        SDLActivity.onNativeResize();

        // Prevent a screen distortion glitch,
        // for instance when the device is in Landscape and a Portrait App is resumed.
        boolean skip = false;
        if (portraitRequested) {
            if (mWidth > mHeight) {
               skip = true;
            }
        } else if (landscapeRequested) {
            if (mWidth < mHeight) {
               skip = true;
            }
        }

        // Special Patch for Square Resolution: Black Berry Passport
        if (skip) {
           double min = Math.min(mWidth, mHeight);
           double max = Math.max(mWidth, mHeight);

           if (max / min < 1.20) {
              Log.v("SDL", "Don't skip on such aspect-ratio. Could be a square resolution.");
              skip = false;
           }
        }

        // Don't skip in MultiWindow.
        if (skip) {
            if (Build.VERSION.SDK_INT >= 24 /* Android 7.0 (N) */) {
                if (SDLActivity.mSingleton.isInMultiWindowMode()) {
                    Log.v("SDL", "Don't skip in Multi-Window");
                    skip = false;
                }
            }
        }

        if (skip) {
           Log.v("SDL", "Skip .. Surface is not ready.");
           mIsSurfaceReady = false;
           return;
        }

        /* If the surface has been previously destroyed by onNativeSurfaceDestroyed, recreate it here */
        SDLActivity.onNativeSurfaceChanged();

        /* Surface is ready */
        mIsSurfaceReady = true;

        SDLActivity.mNextNativeState = SDLActivity.NativeState.RESUMED;
        SDLActivity.handleNativeState();
    }

    // Key events
    @Override
    public boolean onKey(View v, int keyCode, KeyEvent event) {
        return SDLActivity.handleKeyEvent(v, keyCode, event, null);
    }

    // Touch events
    @Override
    public boolean onTouch(View v, MotionEvent event) {
        /* Ref: http://developer.android.com/training/gestures/multi.html */
        int touchDevId = event.getDeviceId();
        final int pointerCount = event.getPointerCount();
        int action = event.getActionMasked();
        int pointerFingerId;
        int i = -1;
        float x,y,p;
        boolean emitTwoFingerRightClick = false;

        if (event.getSource() != InputDevice.SOURCE_MOUSE &&
            event.getSource() != (InputDevice.SOURCE_MOUSE | InputDevice.SOURCE_TOUCHSCREEN)) {
            emitTwoFingerRightClick = updateTwoFingerTap(event);
        }

        /*
         * Prevent id to be -1, since it's used in SDL internal for synthetic events
         * Appears when using Android emulator, eg:
         *  adb shell input mouse tap 100 100
         *  adb shell input touchscreen tap 100 100
         */
        if (touchDevId < 0) {
            touchDevId -= 1;
        }

        // 12290 = Samsung DeX mode desktop mouse
        // 12290 = 0x3002 = 0x2002 | 0x1002 = SOURCE_MOUSE | SOURCE_TOUCHSCREEN
        // 0x2   = SOURCE_CLASS_POINTER
        if (event.getSource() == InputDevice.SOURCE_MOUSE || event.getSource() == (InputDevice.SOURCE_MOUSE | InputDevice.SOURCE_TOUCHSCREEN)) {
            int mouseButton = 1;
            try {
                Object object = event.getClass().getMethod("getButtonState").invoke(event);
                if (object != null) {
                    mouseButton = (Integer) object;
                }
            } catch(Exception ignored) {
            }

            // We need to check if we're in relative mouse mode and get the axis offset rather than the x/y values
            // if we are.  We'll leverage our existing motion listener
            SDLGenericMotionListener_API12 motionListener = SDLActivity.getMotionListener();
            x = motionListener.getEventX(event);
            y = motionListener.getEventY(event);

            SDLActivity.onNativeMouse(mouseButton, action, x, y, motionListener.inRelativeMode());
        } else if (!shouldSuppressTouchEvent(event, touchDevId)) {
            switch(action) {
                case MotionEvent.ACTION_MOVE:
                    for (i = 0; i < pointerCount; i++) {
                        pointerFingerId = event.getPointerId(i);
                        x = event.getX(i) / mWidth;
                        y = event.getY(i) / mHeight;
                        p = event.getPressure(i);
                        if (p > 1.0f) {
                            // may be larger than 1.0f on some devices
                            // see the documentation of getPressure(i)
                            p = 1.0f;
                        }
                        SDLActivity.onNativeTouch(touchDevId, pointerFingerId, action, x, y, p);
                    }
                    break;

                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_DOWN:
                    // Primary pointer up/down, the index is always zero
                    i = 0;
                    /* fallthrough */
                case MotionEvent.ACTION_POINTER_UP:
                case MotionEvent.ACTION_POINTER_DOWN:
                    // Non primary pointer up/down
                    if (i == -1) {
                        i = event.getActionIndex();
                    }

                    pointerFingerId = event.getPointerId(i);
                    x = event.getX(i) / mWidth;
                    y = event.getY(i) / mHeight;
                    p = event.getPressure(i);
                    if (p > 1.0f) {
                        // may be larger than 1.0f on some devices
                        // see the documentation of getPressure(i)
                        p = 1.0f;
                    }
                    SDLActivity.onNativeTouch(touchDevId, pointerFingerId, action, x, y, p);
                    break;

                case MotionEvent.ACTION_CANCEL:
                    for (i = 0; i < pointerCount; i++) {
                        pointerFingerId = event.getPointerId(i);
                        x = event.getX(i) / mWidth;
                        y = event.getY(i) / mHeight;
                        p = event.getPressure(i);
                        if (p > 1.0f) {
                            // may be larger than 1.0f on some devices
                            // see the documentation of getPressure(i)
                            p = 1.0f;
                        }
                        SDLActivity.onNativeTouch(touchDevId, pointerFingerId, MotionEvent.ACTION_UP, x, y, p);
                    }
                    break;

                default:
                    break;
            }
        }

        if (emitTwoFingerRightClick) {
            // Match the coordinates and button/action values used for a real mouse.
            float clickX = (event.getX(0) + event.getX(1)) * 0.5f;
            float clickY = (event.getY(0) + event.getY(1)) * 0.5f;
            SDLActivity.onNativeMouse(MotionEvent.BUTTON_SECONDARY, MotionEvent.ACTION_DOWN,
                                      clickX, clickY, false);
            SDLActivity.onNativeMouse(0, MotionEvent.ACTION_UP,
                                      clickX, clickY, false);
        }

        return true;
   }

    private boolean updateTwoFingerTap(MotionEvent event) {
        final int action = event.getActionMasked();

        if (action == MotionEvent.ACTION_POINTER_DOWN) {
            if (mTwoFingerDragActive) {
                endTwoFingerDrag(event);
            }
            if (event.getPointerCount() == 2 && mPendingSingleTouch) {
                mTwoFingerTapCandidate = true;
                mTwoFingerTapFirstPointerId = event.getPointerId(0);
                mTwoFingerTapSecondPointerId = event.getPointerId(1);
                mTwoFingerTapFirstX = event.getX(0);
                mTwoFingerTapFirstY = event.getY(0);
                mTwoFingerTapSecondX = event.getX(1);
                mTwoFingerTapSecondY = event.getY(1);
                mTwoFingerTapStartTime = event.getEventTime();
                mTwoFingerLastSpan = twoFingerSpan(event, 0, 1);
            } else {
                mTwoFingerTapCandidate = false;
                resetTwoFingerZoom();
            }
            return false;
        }

        if (!mTwoFingerTapCandidate) {
            if (mTwoFingerDragActive && action == MotionEvent.ACTION_MOVE) {
                moveTwoFingerDrag(event);
            }
            if (mTwoFingerDragActive &&
                (action == MotionEvent.ACTION_POINTER_UP || action == MotionEvent.ACTION_UP ||
                 action == MotionEvent.ACTION_CANCEL)) {
                endTwoFingerDrag(event);
            }
            if (action == MotionEvent.ACTION_POINTER_UP || action == MotionEvent.ACTION_UP ||
                action == MotionEvent.ACTION_CANCEL) {
                resetTwoFingerZoom();
            }
            return false;
        }

        if (action == MotionEvent.ACTION_MOVE) {
            int firstIndex = event.findPointerIndex(mTwoFingerTapFirstPointerId);
            int secondIndex = event.findPointerIndex(mTwoFingerTapSecondPointerId);
            if (firstIndex < 0 || secondIndex < 0 ||
                movedBeyondTapSlop(event, firstIndex, mTwoFingerTapFirstX, mTwoFingerTapFirstY) ||
                movedBeyondTapSlop(event, secondIndex, mTwoFingerTapSecondX, mTwoFingerTapSecondY)) {
                mTwoFingerTapCandidate = false;
                beginTwoFingerDrag(event, firstIndex, secondIndex);
                updateTwoFingerZoom(event, firstIndex, secondIndex);
            }
            return false;
        }

        if (action == MotionEvent.ACTION_POINTER_UP && event.getPointerCount() == 2) {
            int firstIndex = event.findPointerIndex(mTwoFingerTapFirstPointerId);
            int secondIndex = event.findPointerIndex(mTwoFingerTapSecondPointerId);
            boolean isTap = event.getEventTime() - mTwoFingerTapStartTime <= ViewConfiguration.getDoubleTapTimeout() &&
                            firstIndex >= 0 && secondIndex >= 0 &&
                            !movedBeyondTapSlop(event, firstIndex, mTwoFingerTapFirstX, mTwoFingerTapFirstY) &&
                            !movedBeyondTapSlop(event, secondIndex, mTwoFingerTapSecondX, mTwoFingerTapSecondY);
            mTwoFingerTapCandidate = false;
            resetTwoFingerZoom();
            return isTap;
        }

        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL ||
            action == MotionEvent.ACTION_POINTER_UP) {
            mTwoFingerTapCandidate = false;
            resetTwoFingerZoom();
        }
        return false;
    }

    private void beginTwoFingerDrag(MotionEvent event, int firstIndex, int secondIndex) {
        if (firstIndex < 0 || secondIndex < 0 || mTwoFingerDragActive) {
            return;
        }
        float x = (event.getX(firstIndex) + event.getX(secondIndex)) * 0.5f;
        float y = (event.getY(firstIndex) + event.getY(secondIndex)) * 0.5f;
        // Move first so the core captures the map anchor at the gesture midpoint.
        SDLActivity.onNativeMouse(0, MotionEvent.ACTION_MOVE, x, y, false);
        SDLActivity.onNativeKeyDown(KeyEvent.KEYCODE_GRAVE);
        mTwoFingerDragActive = true;
    }

    private void moveTwoFingerDrag(MotionEvent event) {
        int firstIndex = event.findPointerIndex(mTwoFingerTapFirstPointerId);
        int secondIndex = event.findPointerIndex(mTwoFingerTapSecondPointerId);
        if (firstIndex < 0 || secondIndex < 0) {
            endTwoFingerDrag(event);
            return;
        }
        float x = (event.getX(firstIndex) + event.getX(secondIndex)) * 0.5f;
        float y = (event.getY(firstIndex) + event.getY(secondIndex)) * 0.5f;
        SDLActivity.onNativeMouse(0, MotionEvent.ACTION_MOVE, x, y, false);
        updateTwoFingerZoom(event, firstIndex, secondIndex);
    }

    private void endTwoFingerDrag(MotionEvent event) {
        if (!mTwoFingerDragActive) {
            return;
        }
        int firstIndex = event.findPointerIndex(mTwoFingerTapFirstPointerId);
        int secondIndex = event.findPointerIndex(mTwoFingerTapSecondPointerId);
        float x = firstIndex >= 0 && secondIndex >= 0
                ? (event.getX(firstIndex) + event.getX(secondIndex)) * 0.5f
                : 0.0f;
        float y = firstIndex >= 0 && secondIndex >= 0
                ? (event.getY(firstIndex) + event.getY(secondIndex)) * 0.5f
                : 0.0f;
        SDLActivity.onNativeKeyUp(KeyEvent.KEYCODE_GRAVE);
        mTwoFingerDragActive = false;
    }

    private void updateTwoFingerZoom(MotionEvent event, int firstIndex, int secondIndex) {
        float span = twoFingerSpan(event, firstIndex, secondIndex);
        float spanDelta = span - mTwoFingerLastSpan;
        mTwoFingerLastSpan = span;
        float zoomDelta = spanDelta / mTwoFingerZoomStep;
        if (zoomDelta == 0.0f) {
            return;
        }

        // SDL retains this fractional value in SDL_MouseWheelEvent.preciseY.
        // One mTwoFingerZoomStep still equals the former one-wheel-tick speed.
        SDLActivity.onNativeMouse(0, MotionEvent.ACTION_SCROLL, 0.0f, zoomDelta, false);
    }

    private float twoFingerSpan(MotionEvent event, int firstIndex, int secondIndex) {
        float deltaX = event.getX(secondIndex) - event.getX(firstIndex);
        float deltaY = event.getY(secondIndex) - event.getY(firstIndex);
        return (float) Math.sqrt(deltaX * deltaX + deltaY * deltaY);
    }

    private void resetTwoFingerZoom() {
        mTwoFingerLastSpan = 0.0f;
    }

    /*
     * SDL turns the first touchscreen finger into a primary mouse button. Hold
     * that down event until we know whether it stays a single-finger tap, so a
     * second finger can turn the entire gesture into a secondary click instead.
     */
    private boolean shouldSuppressTouchEvent(MotionEvent event, int touchDevId) {
        final int action = event.getActionMasked();

        if (action == MotionEvent.ACTION_DOWN) {
            mPendingSingleTouch = true;
            mSuppressTouchSequence = false;
            mPendingTouchDeviceId = touchDevId;
            mPendingTouchPointerId = event.getPointerId(0);
            mPendingTouchX = event.getX(0) / mWidth;
            mPendingTouchY = event.getY(0) / mHeight;
            mPendingTouchPressure = Math.min(event.getPressure(0), 1.0f);
            mPendingTouchDownTime = event.getEventTime();
            return true;
        }

        if (mPendingSingleTouch && action == MotionEvent.ACTION_POINTER_DOWN) {
            mPendingSingleTouch = false;
            mSuppressTouchSequence = true;
            return true;
        }

        if (mPendingSingleTouch && action == MotionEvent.ACTION_MOVE) {
            if (event.getEventTime() - mPendingTouchDownTime < TWO_FINGER_DRAG_GRACE_PERIOD_MS) {
                // Give a second finger time to join before committing this to a one-finger drag.
                return true;
            }
            SDLActivity.onNativeTouch(mPendingTouchDeviceId, mPendingTouchPointerId,
                                      MotionEvent.ACTION_DOWN, mPendingTouchX,
                                      mPendingTouchY, mPendingTouchPressure);
            mPendingSingleTouch = false;
            return false;
        }

        if (mPendingSingleTouch && action == MotionEvent.ACTION_CANCEL) {
            mPendingSingleTouch = false;
            return true;
        }

        if (mPendingSingleTouch && action == MotionEvent.ACTION_UP) {
            SDLActivity.onNativeTouch(mPendingTouchDeviceId, mPendingTouchPointerId,
                                      MotionEvent.ACTION_DOWN, mPendingTouchX,
                                      mPendingTouchY, mPendingTouchPressure);
            mPendingSingleTouch = false;
            return false;
        }

        if (mSuppressTouchSequence) {
            if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
                mSuppressTouchSequence = false;
            }
            return true;
        }

        return false;
    }

    private boolean movedBeyondTapSlop(MotionEvent event, int pointerIndex, float startX, float startY) {
        float deltaX = event.getX(pointerIndex) - startX;
        float deltaY = event.getY(pointerIndex) - startY;
        return deltaX * deltaX + deltaY * deltaY > mTwoFingerTapSlop * mTwoFingerTapSlop;
    }

    // Sensor events
    public void enableSensor(int sensortype, boolean enabled) {
        // TODO: This uses getDefaultSensor - what if we have >1 accels?
        if (enabled) {
            mSensorManager.registerListener(this,
                            mSensorManager.getDefaultSensor(sensortype),
                            SensorManager.SENSOR_DELAY_GAME, null);
        } else {
            mSensorManager.unregisterListener(this,
                            mSensorManager.getDefaultSensor(sensortype));
        }
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int accuracy) {
        // TODO
    }

    @Override
    public void onSensorChanged(SensorEvent event) {
        if (event.sensor.getType() == Sensor.TYPE_ACCELEROMETER) {

            // Since we may have an orientation set, we won't receive onConfigurationChanged events.
            // We thus should check here.
            int newOrientation;

            float x, y;
            switch (mDisplay.getRotation()) {
                case Surface.ROTATION_90:
                    x = -event.values[1];
                    y = event.values[0];
                    newOrientation = SDLActivity.SDL_ORIENTATION_LANDSCAPE;
                    break;
                case Surface.ROTATION_270:
                    x = event.values[1];
                    y = -event.values[0];
                    newOrientation = SDLActivity.SDL_ORIENTATION_LANDSCAPE_FLIPPED;
                    break;
                case Surface.ROTATION_180:
                    x = -event.values[0];
                    y = -event.values[1];
                    newOrientation = SDLActivity.SDL_ORIENTATION_PORTRAIT_FLIPPED;
                    break;
                case Surface.ROTATION_0:
                default:
                    x = event.values[0];
                    y = event.values[1];
                    newOrientation = SDLActivity.SDL_ORIENTATION_PORTRAIT;
                    break;
            }

            if (newOrientation != SDLActivity.mCurrentOrientation) {
                SDLActivity.mCurrentOrientation = newOrientation;
                SDLActivity.onNativeOrientationChanged(newOrientation);
            }

            SDLActivity.onNativeAccel(-x / SensorManager.GRAVITY_EARTH,
                                      y / SensorManager.GRAVITY_EARTH,
                                      event.values[2] / SensorManager.GRAVITY_EARTH);


        }
    }

    // Captured pointer events for API 26.
    public boolean onCapturedPointerEvent(MotionEvent event)
    {
        int action = event.getActionMasked();

        float x, y;
        switch (action) {
            case MotionEvent.ACTION_SCROLL:
                x = event.getAxisValue(MotionEvent.AXIS_HSCROLL, 0);
                y = event.getAxisValue(MotionEvent.AXIS_VSCROLL, 0);
                SDLActivity.onNativeMouse(0, action, x, y, false);
                return true;

            case MotionEvent.ACTION_HOVER_MOVE:
            case MotionEvent.ACTION_MOVE:
                x = event.getX(0);
                y = event.getY(0);
                SDLActivity.onNativeMouse(0, action, x, y, true);
                return true;

            case MotionEvent.ACTION_BUTTON_PRESS:
            case MotionEvent.ACTION_BUTTON_RELEASE:

                // Change our action value to what SDL's code expects.
                if (action == MotionEvent.ACTION_BUTTON_PRESS) {
                    action = MotionEvent.ACTION_DOWN;
                } else { /* MotionEvent.ACTION_BUTTON_RELEASE */
                    action = MotionEvent.ACTION_UP;
                }

                x = event.getX(0);
                y = event.getY(0);
                int button = event.getButtonState();

                SDLActivity.onNativeMouse(button, action, x, y, true);
                return true;
        }

        return false;
    }
}
