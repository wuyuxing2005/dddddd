// MainView.qml
import QtQuick
import QtQuick3D

Item {
    // 这个 Item 会自动填满你在 C++ 里给 QQuickWidget 留的那个黑框
    anchors.fill: parent

    // 建立 3D 渲染视图
    View3D {
        anchors.fill: parent
        
        // 1. 设置摄影棚环境（纯黑背景）
        environment: SceneEnvironment {
            clearColor: "#000000"
            backgroundMode: SceneEnvironment.Color
        }

        // 2. 假设摄像机（观众的眼睛）
        PerspectiveCamera {
            id: camera
            z: 800      // 往后退800，避免镜头贴脸看不见机械臂
            y: 300      // 镜头抬高300
            eulerRotation.x: -15 // 镜头微微低头往下看
        }

        // 3. 打一盏灯光（不然机械臂是黑的）
        DirectionalLight {
            eulerRotation.x: -45
            eulerRotation.y: 45
            brightness: 1.5
        }

        // 4. 召唤演员！(只要 DOBOT_CR3.qml 在同一个文件夹，就可以直接用它的名字)
        DOBOT_CR3 {
            id: myRobot
            // 让机械臂稍微往下沉一点，居中显示
            y: -150 
        }
    }
}