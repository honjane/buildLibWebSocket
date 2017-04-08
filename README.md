# buildLibWebSocket
build libwebsocket

1.下载Libwebseocket库

git clone https://github.com/warmcat/libwebsockets.git

2.环境准备（Mac版）

2.1安装zlib ：
brew install zlib

2.2安装makedepend：

brew install makedepend

2.3安装cmake ：
brew install cmake

3.编译.a静态文件
有了上面这些工具，准备工作差不多了，然后通过libwebsocket/contrib目录下的android-make-script.sh编译.a文件
这个sh文件有些问题，比如：只能编译出arm架构的文件，并且在编译zlib库时使用的libtool有问题，libwebsocket原文件编译出错。

针对这些问题作了一些修改：
3.1 修改output.c和http2.c原文件编译错误，这个已经提交到libwebsocket base2.2版本上了
  https://github.com/warmcat/libwebsockets/commit/34842d7492b728349f6a6898e3893b08d70625fa
3.2 替换libtool文件
    mv ./Makefile ./Makefile.old
    sed "s/AR=libtool/AR=`echo ${AR}|sed 's#\/#\\\/#g'`/" ./Makefile.old > Makefile.mid
    sed "s/ARFLAGS=-o/ARFLAGS=-r/" ./Makefile.mid > Makefile
    
3.3 增加build x86架构文件编译

4.封装websocket.

4.1初始化 initLws

4.2连接  connect

4.3发送／接收消息 writeLws／callback

4.4断开 exitLws

5.搭建简易服务端测试
python testServer.py 
这个py服务器比较简单，只能支持1024字节传输，超过这个长度会乱码，要测试超长字符，服务器还是要自己去搭建。
