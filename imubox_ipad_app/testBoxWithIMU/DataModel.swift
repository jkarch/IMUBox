//
//  DataModel.swift
//  testBox
//
//  Created by Joshua Karch on 9/16/21.
//

import Combine
import RealityKit

final class DataModel: ObservableObject {
    static var shared = DataModel()
    @Published var arView: ARView!
    @Published var enableAR = false
    @Published var xTranslation: Float = 0 {
        didSet {translateRotateBox()}
       }
    @Published var yTranslation: Float = 0 {
        didSet {translateRotateBox()}
       }
    @Published var zTranslation: Float = 0 {
        didSet {translateRotateBox()}
       }
    
    @Published var Qx: Float = 0 {
        didSet {translateRotateBox()}
       }
    @Published var Qy: Float = 0 {
        didSet {translateRotateBox()}
       }
    @Published var Qz: Float = 0 {
        didSet {translateRotateBox()}
       }
    @Published var Real: Float = 0 {
        didSet {translateRotateBox()}
       }
    
    init()  {
        //initialize ARView
        arView = ARView(frame: .zero)
        
        let boxAnchor = try! Experience.loadBox()
        arView.scene.anchors.append(boxAnchor)
    }


    func translateRotateBox() {
        if let steelBox = (arView.scene.anchors[0] as?
            Experience.Box)?.imuCase  {
            let xTranslationM = xTranslation/100
            let yTranslationM = yTranslation/100
            let zTranslationM = zTranslation/100
            
            let translation = SIMD3<Float>(xTranslationM, yTranslationM, zTranslationM)
            let rotation = simd_quaternion(Qx,Qy,Qz,Real)
            
            
            steelBox.transform.translation=translation
            steelBox.transform.rotation=rotation
        }
    }
}
