import QtQuick
import QtQuick3D

Node {
    id: node

    // Resources
    PrincipledMaterial {
        id: base___material
        objectName: "BASE材质"
        baseColor: "#ff97bbe7"
        metalness: 0.699999988079071
        roughness: 0.5
        cullMode: PrincipledMaterial.NoCulling
        alphaMode: PrincipledMaterial.Opaque
    }
    PrincipledMaterial {
        id: j1_J5___material
        objectName: "J1-J5材质"
        baseColor: "#ffb7d3e7"
        metalness: 0.699999988079071
        roughness: 0.5
        cullMode: PrincipledMaterial.NoCulling
        alphaMode: PrincipledMaterial.Opaque
    }
    PrincipledMaterial {
        id: j6___material
        objectName: "J6材质"
        baseColor: "#ffb2e7ca"
        metalness: 0.699999988079071
        roughness: 0.5
        cullMode: PrincipledMaterial.NoCulling
        alphaMode: PrincipledMaterial.Opaque
    }

    // Nodes:
    Model {
        id: base
        objectName: "BASE"
        source: "meshes/obj2_mesh.mesh"
        materials: [
            base___material
        ]
        Model {
            id: j1
            objectName: "J1"
            source: "meshes/obj3_011_mesh.mesh"
            materials: [
                j1_J5___material
            ]
            Model {
                id: j2
                objectName: "J2"
                source: "meshes/obj3_012_mesh.mesh"
                materials: [
                    j1_J5___material
                ]
                Model {
                    id: j3
                    objectName: "J3"
                    source: "meshes/obj1_mesh.mesh"
                    materials: [
                        j1_J5___material
                    ]
                    Model {
                        id: j4
                        objectName: "J4"
                        source: "meshes/obj3_016_mesh.mesh"
                        materials: [
                            j1_J5___material
                        ]
                        Model {
                            id: j5
                            objectName: "J5"
                            source: "meshes/obj1_010_mesh.mesh"
                            materials: [
                                j1_J5___material
                            ]
                            Model {
                                id: j6
                                objectName: "J6"
                                source: "meshes/obj4_001_mesh.mesh"
                                materials: [
                                    j6___material
                                ]
                            }
                        }
                    }
                }
            }
        }
    }

    // Animations:
}
