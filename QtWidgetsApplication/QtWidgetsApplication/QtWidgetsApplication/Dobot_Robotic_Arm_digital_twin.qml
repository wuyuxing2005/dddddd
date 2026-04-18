import QtQuick
import QtQuick3D

Node {
    id: node

    // Resources
    PrincipledMaterial {
        id: base___002_material
        objectName: "BASE材质.002"
        baseColor: "#ff97bbe7"
        metalness: 0.4000000059604645
        roughness: 0.5
        cullMode: PrincipledMaterial.NoCulling
        alphaMode: PrincipledMaterial.Opaque
    }
    PrincipledMaterial {
        id: j1_J5___002_material
        objectName: "J1-J5材质.002"
        baseColor: "#ffe2dce7"
        metalness: 0.4000000059604645
        roughness: 0.5
        cullMode: PrincipledMaterial.NoCulling
        alphaMode: PrincipledMaterial.Opaque
    }

    // Nodes:
    Model {
        id: base
        objectName: "BASE"
        source: "meshes/obj2_002_mesh.mesh"
        materials: [
            base___002_material
        ]
        eulerRotation.y: -90
    }
        Model {
            id: j1
            objectName: "J1"
            source: "meshes/obj3_004_mesh.mesh"
            materials: [
                j1_J5___002_material
            ]
            eulerRotation.y: twinBackend.j1
            Model {
                id: j2
                objectName: "J2"
                position: Qt.vector3d(0.059, 0.0698, 0)
                source: "meshes/obj3_005_mesh.mesh"
                materials: [
                    j1_J5___002_material
                ]
                eulerRotation.x: twinBackend.j2
                Model {
                    id: j3
                    objectName: "J3"
                    position: Qt.vector3d(0.00360003, 0.274, 0)
                    source: "meshes/obj1_003_mesh.mesh"
                    materials: [
                        j1_J5___002_material
                    ]
                    eulerRotation.x: twinBackend.j3
                    Model {
                        id: j4
                        objectName: "J4"
                        position: Qt.vector3d(0.0167034, 0.230261, 0)
                        source: "meshes/obj3_006_mesh.mesh"
                        materials: [
                            j1_J5___002_material
                        ]
                        eulerRotation.x: twinBackend.j4
                        Model {
                            id: j5
                            objectName: "J5"
                            position: Qt.vector3d(0.0490135, 0.0667401, 0)
                            source: "meshes/obj1_004_mesh.mesh"
                            materials: [
                                j1_J5___002_material
                            ]
                            eulerRotation.y: twinBackend.j5
                            Model {
                                id: j6
                                objectName: "J6"
                                position: Qt.vector3d(0.0672829, 0.0487995, 0)
                                source: "meshes/obj4_003_mesh.mesh"
                                materials: [
                                    base___002_material
                                ]
                                eulerRotation.x: twinBackend.j6
                            }
                        }
                    }
                }
            }
        }
    }

    // Animations:

