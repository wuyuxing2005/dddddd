import QtQuick
import QtQuick3D
import QtQuick3D.Helpers // 【关键】必须增加这个引入，才能用网格工具

Item {
    anchors.fill: parent

    View3D {
        anchors.fill: parent
        
        environment: SceneEnvironment {
            clearColor: "#111622" 
            backgroundMode: SceneEnvironment.Color
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
        }

        // ==========================================
        // 【自制万能轨道相机核心】
        Node {
            id: orbitY
            y: 0 
            eulerRotation.y: 90
            Node {
                id: orbitX
                PerspectiveCamera {
                    id: camera
                    z: 1800      
                    y: 800       
                    eulerRotation.x: -20 

                    // 【核心修改 1：矿工头灯】
                    // 把灯光作为 Camera 的子节点，它会永远跟着你的视角转！
                    DirectionalLight {
                        // 稍微偏一点角度（右上方打光），让金属有立体感，不至于太平
                        eulerRotation.x: -10
                        eulerRotation.y: 15
                        brightness: 2.5
                        ambientColor: "#555555" 
                    }
                }
            }
        }
        // ==========================================

        // ==========================================
        // 【浅色系安全版：高亮银灰磨砂地台】
        // 绝对安全！没有任何复杂纹理层，完美适配所有显卡
        Model {
            source: "#Rectangle"
            position: Qt.vector3d(0, -730, 0) 
            scale: Qt.vector3d(50, 50, 1)     
            eulerRotation.x: -90              
            
            materials: [
                PrincipledMaterial {
                    //baseColor: "#C5CBD0"       // 浅灰偏白（类似苹果电脑表面的银灰色）
                    baseColor: "#4A4D54"       // 偏亮的冷灰色
                    metalness: 0.3             // 微弱的金属感，提供扎实的工业氛围
                    roughness: 0.8             // 磨砂质感：让头灯的光晕极其柔和地散开，不刺眼
                    specularAmount: 0.2        // 适当压低高光，防止浅色地板在强光下过曝成纯白
                }
            ]
        }
        // ==========================================

        Dobot_Robotic_Arm_digital_twin {
            id: myRobot
            scale: Qt.vector3d(2000, 2000, 2000) 
            y: -600 
        }
    }

    // ==========================================
    // 【终极鼠标拦截层 - 专治 QQuickWidget 各种不服】
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        
        property real lastX: 0
        property real lastY: 0

        onPressed: (mouse) => {
            lastX = mouse.x
            lastY = mouse.y
        }

       // 鼠标拖动：计算偏移量并旋转/平移相机
        onPositionChanged: (mouse) => {
            let dx = mouse.x - lastX
            let dy = mouse.y - lastY

            if (mouse.buttons & Qt.LeftButton) {
                // 【左键拖拽】：旋转视角
                orbitY.eulerRotation.y -= dx * 0.3
                orbitX.eulerRotation.x -= dy * 0.3
            } else if (mouse.buttons & Qt.RightButton) {
                // 【核心修改：绝对基于视角的二维平移 (Screen-Space Panning)】
                let speed = 1.5

                // 直接获取摄像机在当前 3D 世界里的真实“右方向”和“上方向”向量
                let rightDir = camera.right
                let upDir = camera.up

                // 无论当前处于什么刁钻视角：
                // 鼠标水平移动(dx)：控制中心点沿着摄像机的“右方向”反向移动
                // 鼠标垂直移动(dy)：控制中心点沿着摄像机的“上方向”正向移动
                orbitY.x += (rightDir.x * -dx + upDir.x * dy) * speed
                orbitY.y += (rightDir.y * -dx + upDir.y * dy) * speed
                orbitY.z += (rightDir.z * -dx + upDir.z * dy) * speed
            }

            lastX = mouse.x
            lastY = mouse.y
        }

        onWheel: (wheel) => {
            camera.z -= wheel.angleDelta.y * 1.0
            if (camera.z < 200) camera.z = 200
            if (camera.z > 5000) camera.z = 5000
        }
    }
    // ==========================================
}