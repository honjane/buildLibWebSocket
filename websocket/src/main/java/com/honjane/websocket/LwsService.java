
package com.honjane.websocket;

import android.os.Message;
import android.text.TextUtils;
import android.util.Log;

import java.io.UnsupportedEncodingException;

public class LwsService extends ThreadService {

    /**
     * Commands that can be send to this service
     */
    public final static int MSG_SET_CONNECTION_PARAMETERS = 1;
    public final static int MSG_SEND_MESSAGE = 2;

    /**
     * Messages that may be send to output Messenger
     * Clients should handle these messages.
     **/
    public static final int MSG_CALLBACK_RECEIVE_MESSAGE = 8;
    public static final int MSG_CALLBACK_CLIENT_CONNECTION_ERROR = 1;
    public static final int MSG_CALLBACK_CLIENT_ESTABLISHED = 3;
    //限制发送4096个字节
    private static final int LIMIT_SIZE = 4096;

    public static class ConnectionParameters {
        String serverAddress;
        String serverPath;
        int serverPort;

        ConnectionParameters(
                String serverAddress,
                int serverPort,
                String serverPath
        ) {
            this.serverAddress = serverAddress;
            this.serverPort = serverPort;
            this.serverPath = serverPath;
        }
    }

    /**
     * Handle incoming messages from clients of this service
     */
    @Override
    public void handleInputMessage(Message msg) {
        Message m;
        switch (msg.what) {
            case MSG_SET_CONNECTION_PARAMETERS: {
                ConnectionParameters parameters = (ConnectionParameters) msg.obj;
                setConnectionParameters(
                        parameters.serverAddress,
                        parameters.serverPort,
                        parameters.serverPath
                );
                break;
            }
            case MSG_SEND_MESSAGE:
                sendMessage((String) msg.obj);
                break;
            default:
                super.handleInputMessage(msg);
                break;
        }
    }

    /**
     * The run() function for the thread.
     * For this test we implement a very long lived task
     * that sends many messages back to the client.
     **/
    public void workerThreadRun() {

        initLws();
        connectLws();

        while (true) {

            // service the websockets
            serviceLws(0);

            // Check if we must quit or suspend
            synchronized (mThreadLock) {
                while (mMustSuspend) {
                    // We are asked to suspend the thread
                    try {
                        mThreadLock.wait();

                    } catch (InterruptedException e) {
                    }
                }
                if (mMustQuit) {
                    // The signal to quit was given
                    break;
                }
            }

            // Throttle the loop so that it iterates once every 50ms
            try {
                Thread.sleep(50);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }

        }
        exitLws();
    }

    /** Load the native libwebsockets code */
    static {
        try {
            System.loadLibrary("hjwebsocket");
        } catch (UnsatisfiedLinkError ule) {
            Log.e("LwsService", "Warning: Could not load native library: " + ule.getMessage());
        }
    }

    public native boolean initLws();

    public native void exitLws();

    public native void serviceLws(int timeout);

    /**
     * 设置连接参数
     * required， before initLws
     * @param serverAddress 服务器地址
     * @param serverPort    端口号
     * @param path          路径
     */
    public native void setConnectionParameters(String serverAddress, int serverPort, String path);

    //optional， If the call must before init
    public native void setTimeout(int timeout);

    //optional， If the call must before init
    public native void setPingInterval(int interval);

    //optional， If the call must before init
    public native void setCaCert(String ca, String cert, String certkey);

    public native boolean connectLws();

    //注意：直接调用可能会出现线程安全问题
    private static native int sendMessageLws(String message, boolean isWs);

    /**
     * ws连接阻塞
     *
     * @return 1阻塞；0 非阻塞
     */

    public native int sendChokedLws();

    /**
     * @param message
     * @return 0 失败 ；大于0 成功
     */
    public synchronized int sendMessage(String message) {
        if (TextUtils.isEmpty(message)) {
            return 0;
        }
        try {
            if (message.getBytes("utf-8").length > LIMIT_SIZE) {
                return 0;
            }
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
        }
        return sendMessageLws(message, true);
    }

}
