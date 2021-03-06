import * as arduino from "./arduino";

// By growing our Wasm Memory by 1 page (64KB)
memory.grow(1);

//let axisNumber:i32 = 0;
//let axisValue:f32 = 0;
let axesArray:Array<f32> = [0,0,0,0,0,0,0];
let distance:f32 = 0;
let errorFreeJson:bool = true;
let errorFreeData:bool = true;
let axesDataCounter = 0;
let axesJsonCounter = 0;
const SAFETY_DISTANCE :f64 = 1500;
const INVALID_FLAG:f32 = 1;

//TODO: define more efficient offset
const AXES_ARRAY_OFFSET = 0; //from 0 to 55 for an array with length 7 (Axis1-7)
const A2_IN_A1_COORD_OFFSET = 56; //56-67, i32
const A3_IN_A2_COORD_OFFSET = 68; //68- 79
const A4_IN_A3_COORD_OFFSET = 80; //80-91
const TIP_ARRAY_OFFSET = 92;//92-123
const DUMMY_OBJECT_COORD_OFFSET = 124;//124-147
const DISTANCE_RESULT_OFFSET = 148;//148-155
const HT_MATRIX_OFFSET01 = 200;//200-327
const HT_MATRIX_OFFSET12 = 350;//350-477
const HT_MATRIX_OFFSET23 = 500;//500-627
const HT_MATRIX_OFFSET34 = 650;//650-777
const HT_MATRIX_OFFSET45 = 800;//800-927
const TIP_COORD_OFFSET = 950;//950-981 Array<f64>
const MATRIX_RESULT_OFFSET0112 = 1000;//1000-1127
const MATRIX_RESULT_OFFSET1223 = 1150;//1150-1277
const MATRIX_RESULT_OFFSET2334 = 1300;//1300-1427
const MATRIX_RESULT_OFFSET3445 = 1450;//1450-1577

/*
let posA2fromA1 = [350, 750, 0] //coordinate of A2 in A1 coordinate system (mm)
let posA3fromA2 = [1250, 0, 0]
let posA4fromA3 = [1100, 0, 0]
let tipFromA5 = [230,0,0,1] //coordinate of tip in the A5-axis coordinate system. With an accessory, one needs tipFrom6 in the A6-axis coordinate system instead of.
let objectCoordinate = [1680, 2000, 1499]*/

export function initAxesCoordinates():void{
  let sizeOfValue = 4;//i32
  let sizeOfValuef64 = 8;//f64
  store<i32>(A2_IN_A1_COORD_OFFSET,350) //A2's coordinate in A1 coordinate system: (350, 750, 0)
  store<i32>(A2_IN_A1_COORD_OFFSET+sizeOfValue,750)
  store<i32>(A2_IN_A1_COORD_OFFSET+sizeOfValue*2,0)
  store<i32>(A3_IN_A2_COORD_OFFSET,1250) //A3 coordinate in A2 coordinate system: (1250, 0, 0)
  store<i32>(A3_IN_A2_COORD_OFFSET+sizeOfValue,0)
  store<i32>(A3_IN_A2_COORD_OFFSET+sizeOfValue*2,0)
  store<i32>(A4_IN_A3_COORD_OFFSET,1100) //A4 coordinate in A3 coordinate system: (1100, 0, 0)
  store<i32>(A4_IN_A3_COORD_OFFSET+sizeOfValue,0)
  store<i32>(A4_IN_A3_COORD_OFFSET+sizeOfValue*2,0)
  store<f64>(TIP_ARRAY_OFFSET,230) //Tip coordinate in A5 coordinate system: (230, 0, 0) without accessories, p_A5 = (230,0,0,1) in Homogeneous Transformation
  store<f64>(TIP_ARRAY_OFFSET+sizeOfValuef64,0)
  store<f64>(TIP_ARRAY_OFFSET+sizeOfValuef64*2,0)
  store<f64>(TIP_ARRAY_OFFSET+sizeOfValuef64*3,1)
  store<f64>(DUMMY_OBJECT_COORD_OFFSET,1680) //Dummy object point (1680, 2000, 1499)
  store<f64>(DUMMY_OBJECT_COORD_OFFSET+sizeOfValuef64,2000)
  store<f64>(DUMMY_OBJECT_COORD_OFFSET+sizeOfValuef64*2,1499)
}



/*export function convertNumber(dataArray:Uint8Array):i32{
  let convertedValue:i32=0
  for(let i:i32 = 3; i>=0;i--){
    convertedValue = (convertedValue<<8) + dataArray[i]
  }

  return  convertedValue;
}*/

function convertNum(num1:u8,num2:u8,num3:u8,num4:u8):i32{
  let convertedValue:i32= num4
  convertedValue = (convertedValue<<8) + num3
  convertedValue = (convertedValue<<8) + num2
  convertedValue = (convertedValue<<8) + num1
  return convertedValue;
}

function isAxisValueValid(number:i32, value:f64):bool{

  switch (number) {
    case 1:
      return (-185<=value && value<=185);
    case 2:
      return (-146<=value && value<=0);
    case 3:
      return (-119<=value && value<=155);
    case 4:
      return (-350<=value && value<=350);
    case 5:
      return (-125<=value && value<=125);
    case 6:
      return (-350<=value && value<=350);
    case 7: //TODO: Add checking external axis (linear, 7th axis)
      return true;
    default: {
      return false;
    }
  }
}


//1st byte is the id of axes. It returns the value in the JSON format.
export function dataProcessWasm(axisnum1:u8,axisnum2:u8,axisnum3:u8,axisnum4:u8,sign:u8, axisval1:u8,axisval2:u8,axisval3:u8,axisval4:u8):void{

  axesJsonCounter++

  if(!errorFreeJson){
    if(axesJsonCounter==7){
      errorFreeJson = true
      axesJsonCounter = 0
    }
    let invalidKey:string = "invalid"
    arduino.jsonEncoder(changetype<usize>(invalidKey), invalidKey.length, 0, INVALID_FLAG)
    return;
  }

  let axisNumber = convertNum(axisnum1,axisnum2,axisnum3,axisnum4)
  let axisValue = (f32)(convertNum(axisval1,axisval2,axisval3,axisval4)/100000.0)
  //axisValue = (f32)(convertNum(axisval1,axisval2,axisval3,axisval4))

  if(axisNumber<1 || axisNumber>7 || !isAxisValueValid(axisNumber,axisValue)){

    if(axesJsonCounter==7){
      axesJsonCounter = 0
    }
    else{
      errorFreeJson = false
    }

    let invalidKey:string = "invalid"
    arduino.jsonEncoder(changetype<usize>(invalidKey), invalidKey.length, 0, INVALID_FLAG)
  }
  else{
    let key:string = "axis_"
    arduino.jsonEncoder(changetype<usize>(key), key.length, axisNumber, axisValue)

    if(axesJsonCounter==7){
      axesJsonCounter = 0
    }
  }

}


function storeAxisValue(number:i32,value:f32): void{
  axesArray[number] = value
}

/*
  Reverse positive and negative angles to copy them in coordinate system.
  It assumes that the robot's base stands on the ground, and the initial position of the tip is positive (on the x-y plane).
  Therefore, the clockwise rotation is negative, the counterclockwise is positive, but KUKA robots outputs the opposite
  */
export function setAxisData(axisnum1:u8,axisnum2:u8,axisnum3:u8,axisnum4:u8,sign:u8, axisval1:u8,axisval2:u8,axisval3:u8,axisval4:u8):i32{

  axesDataCounter++

  if(!errorFreeData){
    if(axesDataCounter==7){
      errorFreeData = true
      axesDataCounter = 0
    }
    return 0
  }

  let axisNumber = convertNum(axisnum1,axisnum2,axisnum3,axisnum4)
  let axisValue = (f32)(convertNum(axisval1,axisval2,axisval3,axisval4)/100000.0)

  if(axisNumber<1 || axisNumber>7 || !isAxisValueValid(axisNumber,axisValue)){
    if(axisNumber==7){
      axesDataCounter = 0
    }
    else{
      errorFreeData = false
    }
      return 0
  }
  else{
    let sizeOfValue = 8;//f64
    if(axisNumber == 7){
      store<f64>(AXES_ARRAY_OFFSET+(axisNumber-1)*sizeOfValue,-axisValue)//Reverse the sign of the value
      axesDataCounter = 0
      calcDistance()
    }
    else{
      store<f64>(AXES_ARRAY_OFFSET+(axisNumber-1)*sizeOfValue,-axisValue)//Reverse the sign of the value
    }
    return axisNumber
  }
}

/*
export function getMinDistance():f32{
  return getDistance()
}


export function isUnsafe():bool{
  return isCollisionDetected()
}*/


//Homogeneous Transformation Matrix for Axis 1-5
//TODO: ?????????????????????????????????????????????????????????<Array<Array>>, f64[][], Array<f64[]>???????????????????????????:??????????????????????????????????????????:?????????Arduino????????????????????????????????????????????????:Arduino???????????????????????????????????????????????????????????????????????????????????????????????????????????????


//TODO: Consider the movement on the rail (the matrix T01)
//TODO: Put some points on each parts of arm
//TODO: Value of Axis 6 is needed, if a parts is attached on the tip.
//Create a Homogeneous Transformation Matrix
//Calling with variable number of args better...
function storef64ArrayRaw4x4(offset:i32,value1:f64,value2:f64,value3:f64,value4:f64):void{
  store<f64>(offset,value1)
  store<f64>(offset+8,value2)
  store<f64>(offset+16,value3)
  store<f64>(offset+24,value4)
}

function setHTMatrix(axisNumber:i32, axisValue:f64):void{
  let arraySize = 4;
  let radian:f64 = axisValue*Math.PI/180
  let typeSize = 8; //size of f64
  let rawOffset2 = typeSize*arraySize //offset with f64 array of length 4
  let rawOffset3 = 2*typeSize*arraySize
  let rawOffset4 = 3*typeSize*arraySize
  if(axisNumber==1){
    //rotation in the y-dimension
    //Here assumes that there is no translation on the rail
    storef64ArrayRaw4x4(HT_MATRIX_OFFSET01,Math.cos(radian), 0, Math.sin(radian), 0)
    storef64ArrayRaw4x4(HT_MATRIX_OFFSET01+rawOffset2,0, 1, 0, 0)
    storef64ArrayRaw4x4(HT_MATRIX_OFFSET01+rawOffset3,-Math.sin(radian), 0, Math.cos(radian), 0)
    storef64ArrayRaw4x4(HT_MATRIX_OFFSET01+rawOffset4,0, 0, 0, 1)
  }
  else if(axisNumber == 4){
    //rotation in the x-dimension
    storef64ArrayRaw4x4(HT_MATRIX_OFFSET34,1, 0, 0, load<i32>(A4_IN_A3_COORD_OFFSET))
    storef64ArrayRaw4x4(HT_MATRIX_OFFSET34+rawOffset2,0,Math.cos(radian), -Math.sin(radian), load<i32>(A4_IN_A3_COORD_OFFSET+4))
    storef64ArrayRaw4x4(HT_MATRIX_OFFSET34+rawOffset3,0,Math.sin(radian), Math.cos(radian), load<i32>(A4_IN_A3_COORD_OFFSET+8))
    storef64ArrayRaw4x4(HT_MATRIX_OFFSET34+rawOffset4,0, 0, 0, 1)
  }
  else if (axisNumber == 2){
    //rotation in the z-dimension
    storef64ArrayRaw4x4(HT_MATRIX_OFFSET12,Math.cos(radian), -Math.sin(radian), 0, load<i32>(A2_IN_A1_COORD_OFFSET))
    storef64ArrayRaw4x4(HT_MATRIX_OFFSET12+rawOffset2,Math.sin(radian), Math.cos(radian), 0, load<i32>(A2_IN_A1_COORD_OFFSET+4))
    storef64ArrayRaw4x4(HT_MATRIX_OFFSET12+rawOffset3,0, 0, 1, load<i32>(A2_IN_A1_COORD_OFFSET+8))
    storef64ArrayRaw4x4(HT_MATRIX_OFFSET12+rawOffset4,0, 0, 0, 1)
    }
  else if(axisNumber == 3){
    storef64ArrayRaw4x4(HT_MATRIX_OFFSET23,Math.cos(radian), -Math.sin(radian), 0, load<i32>(A3_IN_A2_COORD_OFFSET))
    storef64ArrayRaw4x4(HT_MATRIX_OFFSET23+rawOffset2,Math.sin(radian), Math.cos(radian), 0, load<i32>(A3_IN_A2_COORD_OFFSET+4))
    storef64ArrayRaw4x4(HT_MATRIX_OFFSET23+rawOffset3,0, 0, 1, load<i32>(A3_IN_A2_COORD_OFFSET+8))
    storef64ArrayRaw4x4(HT_MATRIX_OFFSET23+rawOffset4,0, 0, 0, 1)
  }
  else if(axisNumber == 5) {
    //It assumes that A4 and A5 are on the same coordinate.
    storef64ArrayRaw4x4(HT_MATRIX_OFFSET45,Math.cos(radian), -Math.sin(radian), 0, 0)
    storef64ArrayRaw4x4(HT_MATRIX_OFFSET45+rawOffset2,Math.sin(radian), Math.cos(radian), 0, 0)
    storef64ArrayRaw4x4(HT_MATRIX_OFFSET45+rawOffset3,0, 0, 1, 0)
    storef64ArrayRaw4x4(HT_MATRIX_OFFSET45+rawOffset4,0, 0, 0, 1)
  }

}

function matrixMultiplication(matrixOffset1:i32,matrixOffset2:i32,resultOffset:i32):void{
  const sizeOfVariable = 8;//f64
  let counter = 0
  let result:f64 = 0
//TODO: implement matrix multiplication with offset
  for(let i=0; i<4; i++){
    for(let j=0; j<4; j++){
      for(let k=0; k<4; k++){
        //result[i][j] += matrix1[i][k]*matrix2[k][j]
        result += load<f64>(matrixOffset1 + i*4*sizeOfVariable + k*sizeOfVariable) * load<f64>(matrixOffset2 + k*4*sizeOfVariable + j*sizeOfVariable)
      }
      store<f64>(resultOffset+sizeOfVariable*counter,result)
      /*arduino.showArrayRaw(<f32>load<f64>(resultOffset),<f32>load<f64>(resultOffset+8),<f32>load<f64>(resultOffset+2*8),<f32>load<f64>(resultOffset+3*8))
      arduino.showArrayRaw(<f32>load<f64>(resultOffset+4*8),<f32>load<f64>(resultOffset+5*8),<f32>load<f64>(resultOffset+6*8),<f32>load<f64>(resultOffset+7*8))
      arduino.showArrayRaw(<f32>load<f64>(resultOffset+8*8),<f32>load<f64>(resultOffset+9*8),<f32>load<f64>(resultOffset+10*8),<f32>load<f64>(resultOffset+11*8))
      arduino.showArrayRaw(<f32>load<f64>(resultOffset+12*8),<f32>load<f64>(resultOffset+13*8),<f32>load<f64>(resultOffset+14*8),<f32>load<f64>(resultOffset+15*8))*/
      result = 0
      counter++
    }
  }
}

function calcTipCoordinate():void{
  let result:f64 = 0
  let sizeOfVariable = 8;//f64
  //Calculate T01*T12*T23*T34*T45*tipFromA5
  //arduino.setAxis(100,<f32>load<f64>(HT_MATRIX_OFFSET12+8*7))
  //TODO: ????????????????????????????????????. ?????????????????????????????????????????????$HOME/Library/Arduino15???Preferences???????????????????????????/Library/Arduino15/packages/esp32/hardware/esp32/1.0.4/cores/esp32???main.cpp???xTaskCreateUniversal?????????????????????????????????
  matrixMultiplication(HT_MATRIX_OFFSET01,HT_MATRIX_OFFSET12,MATRIX_RESULT_OFFSET0112)
  //arduino.setAxis(100,<f32>load<f64>(MATRIX_RESULT_OFFSET+8*3))
  /*arduino.showArrayRaw(<f32>load<f64>(MATRIX_RESULT_OFFSET),<f32>load<f64>(MATRIX_RESULT_OFFSET+8),<f32>load<f64>(MATRIX_RESULT_OFFSET+2*8),<f32>load<f64>(MATRIX_RESULT_OFFSET+3*8))
  arduino.showArrayRaw(<f32>load<f64>(MATRIX_RESULT_OFFSET+4*8),<f32>load<f64>(MATRIX_RESULT_OFFSET+5*8),<f32>load<f64>(MATRIX_RESULT_OFFSET+6*8),<f32>load<f64>(MATRIX_RESULT_OFFSET+7*8))
  arduino.showArrayRaw(<f32>load<f64>(MATRIX_RESULT_OFFSET+8*8),<f32>load<f64>(MATRIX_RESULT_OFFSET+9*8),<f32>load<f64>(MATRIX_RESULT_OFFSET+10*8),<f32>load<f64>(MATRIX_RESULT_OFFSET+11*8))
  arduino.showArrayRaw(<f32>load<f64>(MATRIX_RESULT_OFFSET+12*8),<f32>load<f64>(MATRIX_RESULT_OFFSET+13*8),<f32>load<f64>(MATRIX_RESULT_OFFSET+14*8),<f32>load<f64>(MATRIX_RESULT_OFFSET+15*8))*/
  matrixMultiplication(MATRIX_RESULT_OFFSET0112,HT_MATRIX_OFFSET23,MATRIX_RESULT_OFFSET1223)
  matrixMultiplication(MATRIX_RESULT_OFFSET1223,HT_MATRIX_OFFSET34,MATRIX_RESULT_OFFSET2334)
  matrixMultiplication(MATRIX_RESULT_OFFSET2334,HT_MATRIX_OFFSET45,MATRIX_RESULT_OFFSET3445)
  //arduino.setAxis(100,<f32>load<f64>(MATRIX_RESULT_OFFSET+3*8))

  for(let i=0; i<4; i++){
    for(let j=0; j<4; j++){
        //coordinate[i] += storeMatrix[i][j]*tipFromA5[j]
        result += load<f64>(MATRIX_RESULT_OFFSET3445 + i*4*sizeOfVariable + j*sizeOfVariable) * load<f64>(TIP_ARRAY_OFFSET+j*sizeOfVariable)
    }
    store<f64>(TIP_COORD_OFFSET+i*sizeOfVariable,result)
    //arduino.showArrayRaw(<f32>load<f64>(TIP_COORD_OFFSET),<f32>load<f64>(TIP_COORD_OFFSET+8),<f32>load<f64>(TIP_COORD_OFFSET+2*8),<f32>load<f64>(TIP_COORD_OFFSET+3*8))
    result = 0
  }
}


//This function called if 1-7 axis data are ready
export function calcDistance():void {
  let radValue:f64 = 0;
  let sizeOfVariable = 8;//f64
  for (let i = 0; i < 7; i++) {
    radValue = load<f64>(AXES_ARRAY_OFFSET+i*sizeOfVariable)
    //setHTMatrix(i+1, load<f64>(AXES_ARRAY_OFFSET+i*sizeOfVariable)*3.14159/180)//radian
    setHTMatrix(i+1, radValue)
  }

    calcTipCoordinate()
    //arduino.setAxis(100,<f32>load<f64>(TIP_COORD_OFFSET))

    arduino.showArrayRaw(<f32>load<f64>(TIP_COORD_OFFSET),<f32>load<f64>(TIP_COORD_OFFSET+8),<f32>load<f64>(TIP_COORD_OFFSET+2*8),<f32>load<f64>(TIP_COORD_OFFSET+3*8))
    let coordinateX:f64 = load<f64>(TIP_COORD_OFFSET)
    let coordinateY:f64 = load<f64>(TIP_COORD_OFFSET+sizeOfVariable)
    let coordinateZ:f64 = load<f64>(TIP_COORD_OFFSET+2*sizeOfVariable)
    let dummyX:f64 = load<f64>(DUMMY_OBJECT_COORD_OFFSET)
    let dummyY:f64 = load<f64>(DUMMY_OBJECT_COORD_OFFSET+sizeOfVariable)
    let dummyZ:f64 = load<f64>(DUMMY_OBJECT_COORD_OFFSET+2*sizeOfVariable)
    //arduino.showArrayRaw(<f32>load<f64>(DUMMY_OBJECT_COORD_OFFSET),<f32>load<f64>(DUMMY_OBJECT_COORD_OFFSET+8),<f32>load<f64>(DUMMY_OBJECT_COORD_OFFSET+2*8),<f32>load<f64>(DUMMY_OBJECT_COORD_OFFSET+2*8))
    store<f64>(DISTANCE_RESULT_OFFSET,Math.sqrt((Math.pow(coordinateX-dummyX,2)+Math.pow(coordinateY-dummyY,2)+Math.pow(coordinateZ-dummyZ,2))))
}

export function getDistance():f64{
  //return distance
  return load<f64>(DISTANCE_RESULT_OFFSET)
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
/*
export function getTip():f64{
  return tip[0]
}*/

export function setNetConfigJson():void{
  //mqtt_server, ssid, password, mqtt_user, mqtt_password, mqtt_port
  //let jsonString:string = '{\"mqtt_server\": \"XX.XX.XX.XX\", \"ssid\": \"YOUR_ID\",\"password\": \"NETWORK_PASSWD\",\"mqtt_user\": \"MQTT_USER\",\"mqtt_password\": \"MQTT_PASSWD\",\"mqtt_port\": 1883}'
  let jsonString:string = '{\"mqtt_server\": \"192.168.178.52\", \"ssid\": \"FRITZ!Box 7560 YQ\",\"password\": \"19604581320192568195\",\"mqtt_user\": \"wasmretrofitting\",\"mqtt_password\": \"wasmretrofitting\",\"mqtt_port\": 1883}'
  arduino.setConfJson(changetype<usize>(jsonString), jsonString.length)
}