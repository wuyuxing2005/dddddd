import QtQuick 2.15
import QtQuick.Controls 2.15

// 这是纯 2D 的视频显示框，没有任何 3D 负担
Rectangle {
    anchors.fill: parent
    color: "#111622" // 和你 3D 背景保持一致的深色

    Image {
        id: videoDisplay
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        
        // 这里填你那个 NO SIGNAL 图片的路径
        source: "qrc:/QtWidgetsApplication/no_signal.jpg" 
        cache: false 
        
        function reloadFrame() {
            source = "image://liveStream/frame?id=" + Math.random()
        }
    }

    Connections {
        target: videoReceiver // 监听 C++ 传来的信号
        function onNewFrameReceived() {
            videoDisplay.reloadFrame()
        }
    }
}