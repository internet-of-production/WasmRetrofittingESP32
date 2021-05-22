import * as arduino from "./arduino";

//Homogeneous Transformation Matrix for Axis 1-5
let T01:Array<Array<f64>> = [[0],[0],[0],[0]] //Transformation matrix from the origin to Axis1
let T12:Array<Array<f64>> = [[0],[0],[0],[0]] //Transformation matrix from Axis1 to Axis2
let T23:Array<Array<f64>> = [[0],[0],[0],[0]]
let T34:Array<Array<f64>> = [[0],[0],[0],[0]]
let T45:Array<Array<f64>> = [[0],[0],[0],[0]]
let posA2fromA1 = [350, 750, 0] //coordinate of A2 in A1 coordinate system (mm)
let posA3fromA2 = [1250, 0, 0]
let posA4fromA3 = [1100, 0, 0]
let tipFromA5 = [230,0,0,1] //coordinate of tip in the A5-axis coordinate system. With an accessory, one needs tipFrom6 in the A6-axis coordinate system instead of.
let objectCoordinate = [1680, 2000, 1499]
let distance:f64 = 0
let tip:Array<f64> = [0.0,0.0,0.0]
const SAFETY_DISTANCE :f64 = 1500


//TODO: Consider the movement on the rail (the matrix T01)
//TODO: Put some points on each parts of arm
//TODO: Value of Axis 6 is needed, if a parts is attached on the tip.
//Create a Homogeneous Transformation Matrix
function setHTMatrix(axisNumber:i32, axisValue:f64):void{
    if(axisNumber==1){
        //rotation in the y-dimension
        //Here assumes that there is no translation on the rail
        let raw1:Array<f64> = [arduino.cos(axisValue), 0, arduino.sin(axisValue), 0]
        let raw2:Array<f64> = [0, 1, 0, 0]
        let raw3:Array<f64> = [-arduino.sin(axisValue), 0, arduino.cos(axisValue), 0]
        let raw4:Array<f64> = [0, 0, 0, 1]
        T01[0] = raw1
        T01[1] = raw2
        T01[2] = raw3
        T01[3] = raw4
    }
    else if(axisNumber == 4){
        //rotation in the x-dimension
        let raw1:Array<f64> = [1, 0, 0, posA4fromA3[0]]
        let raw2:Array<f64> = [0, arduino.cos(axisValue), -arduino.sin(axisValue), posA4fromA3[1]]
        let raw3:Array<f64> = [0, arduino.sin(axisValue), arduino.cos(axisValue), posA4fromA3[2]]
        let raw4:Array<f64> = [0, 0, 0, 1]
        T34[0] = raw1
        T34[1] = raw2
        T34[2] = raw3
        T34[3] = raw4
    }
    else {
        //rotation in the z-dimension
        let raw1:Array<f64> = [arduino.cos(axisValue), -arduino.sin(axisValue), 0, 0]
        let raw2:Array<f64> = [arduino.sin(axisValue), arduino.cos(axisValue), 0, 0]
        let raw3:Array<f64> = [0, 0, 1, 0]
        let raw4:Array<f64> = [0, 0, 0, 1]
        if(axisNumber == 2){
            raw1[3] = posA2fromA1[0]
            raw2[3] = posA2fromA1[1]
            raw3[3] = posA2fromA1[2]
            T12[0] = raw1
            T12[1] = raw2
            T12[2] = raw3
            T12[3] = raw4
        }
        else if(axisNumber == 3){
            raw1[3] = posA3fromA2[0]
            raw2[3] = posA3fromA2[1]
            raw3[3] = posA3fromA2[2]
            T23[0] = raw1
            T23[1] = raw2
            T23[2] = raw3
            T23[3] = raw4
        }
        else if(axisNumber == 5) {
            //It assumes that A4 and A5 are on the same coordinate.
            T45[0] = raw1
            T45[1] = raw2
            T45[2] = raw3
            T45[3] = raw4
        }
    }
}

function matrixMultiplication(matrix1:Array<Array<f64>>,matrix2:Array<Array<f64>>):Array<Array<f64>>{
    //let initArray:Array<f64> = [0, 0, 0, 0]
    let result:Array<Array<f64>> = [[0,0,0,0],[0,0,0,0],[0,0,0,0],[0,0,0,0]]

    for(let i=0; i<4; i++){
        for(let j=0; j<4; j++){
            for(let k=0; k<4; k++)
            result[i][j] += matrix1[i][k]*matrix2[k][j]
        }
    }
    return result
}
function calcTipCoordinate():Array<f64>{
    //Calculate T01*T12*T23*T34*T45*tipFromA5
    let initArray:Array<f64> = [0, 0, 0, 0]
    let coordinate:Array<f64> = [0, 0, 0, 0]
    let storeMatrix:Array<Array<f64>> = [initArray.slice(0),initArray.slice(0),initArray.slice(0),initArray.slice(0)]
    /*storeMatrix = matrixMultiplication(T01,T12).slice(0)
    storeMatrix = matrixMultiplication(storeMatrix,T23).slice(0)
    storeMatrix = matrixMultiplication(storeMatrix,T34).slice(0)
    storeMatrix = matrixMultiplication(storeMatrix,T45).slice(0)*/
    storeMatrix = matrixMultiplication(matrixMultiplication(matrixMultiplication(matrixMultiplication(T01,T12),T23),T34),T45)

    for(let i=0; i<4; i++){
        for(let j=0; j<4; j++){
                coordinate[i] += storeMatrix[i][j]*tipFromA5[j]
        }
    }

    return coordinate
}

//This function called if 1-7 axis data are ready
export function calcDistance(axes:Array<f64>):void {

    for (let i = 0; i < 7; i++) {
        setHTMatrix(i+1, axes[i])
    }

    let coordinate:Array<f64> = calcTipCoordinate()
    tip = coordinate
    distance = arduino.sqrt(arduino.pow(coordinate[0]-objectCoordinate[0],2)+arduino.pow(coordinate[1]-objectCoordinate[1],2)+arduino.pow(coordinate[2]-objectCoordinate[2],2))
}

export function getDistance():f64{
    return distance
}

/*TODO: Check SAFETY_DISTANCE. According to the previous thesis: D = 1097.2mm + (2472 ms)*max_robot_speed(mm/ms)
For max_robot_speed = 1 (mm/ms), D = 3569.2 mm
*/
export function isCollisionDetected():bool{
    if(distance<SAFETY_DISTANCE){
        return true
    }
    return false
}

export function getTip():f64{
    return tip[0]
}