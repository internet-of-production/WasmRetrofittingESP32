import * as arduino from "./arduino";


let axisNumber:i32 = 0;
let axisValue:f32 = 0;


export function add(a: i32, b: i32): void {
  let result = a+b;
  store<i32>(0,result)
}


export function convertNumber(dataArray:Uint8Array):i32{
  let convertedValue:i32=0
  for(let i:i32 = 3; i>=0;i--){
    convertedValue = (convertedValue<<8) + dataArray[i]
  }

  return  convertedValue;
}

function convertNum(num1:u8,num2:u8,num3:u8,num4:u8):i32{
  let convertedValue:i32= num4
  convertedValue = (convertedValue<<8) + num3
  convertedValue = (convertedValue<<8) + num2
  convertedValue = (convertedValue<<8) + num1
  return convertedValue;
}

//1st byte is the id of axes. It returns the value in the JSON format.
export function dataProcessWasm(axisnum1:u8,axisnum2:u8,axisnum3:u8,axisnum4:u8,sign:u8, axisval1:u8,axisval2:u8,axisval3:u8,axisval4:u8):void{

  axisNumber = convertNum(axisnum1,axisnum2,axisnum3,axisnum4)
  axisValue = (f32)(convertNum(axisval1,axisval2,axisval3,axisval4)/100000.0)

  if(axisNumber<0 || axisNumber>7){
    axisNumber = -1
  }

  arduino.setAxis(axisNumber,axisValue)

}