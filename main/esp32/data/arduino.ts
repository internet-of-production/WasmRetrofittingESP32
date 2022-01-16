// @ts-ignore: decorator
@external("setAxis")
export declare function setAxis(number:i32, value:f32): void;
// @ts-ignore: decorator
@external("showArrayRaw")
export declare function showArrayRaw(v1:f32,v2:f32,v3:f32,v4:f32): void;


// @ts-ignore: decorator
@external("printUTF16")
export declare function printUTF16(ptr: usize, len: usize): void;

export function print(str: string): void {
    printUTF16(changetype<usize>(str), str.length);
}

export function println(str: string): void {
    print(str);
    print('\n');
}

// @ts-ignore: decorator
@external("jsonEncoder")
export declare function jsonEncoder(ptr: usize, len: usize, number: i32, value: f32): void;

// @ts-ignore: decorator
@external("setConfJson")
export declare function setConfJson(ptr: usize, len: usize): void;