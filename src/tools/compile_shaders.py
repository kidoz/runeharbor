#!/usr/bin/env python3
import os
import subprocess
import argparse

def compile_glsl(input_path):
    # Compiles GLSL to SPIR-V using glslangValidator
    base = os.path.splitext(input_path)[0]
    ext = os.path.splitext(input_path)[1]
    spv_path = base + ext + ".spv"
    
    try:
        subprocess.check_call(["glslangValidator", "-V", input_path, "-o", spv_path])
        with open(spv_path, "rb") as f:
            data = f.read()
        os.remove(spv_path)
        return data, "SPIRV_COMPILED"
    except Exception as e:
        print(f"Warning: Failed to compile {input_path} with glslangValidator: {e}")
        # Fallback to empty if it fails
        return b"", "FAILED"

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', required=True)
    parser.add_argument('inputs', nargs='+')
    args = parser.parse_args()

    header_content = """// Auto-generated file. Do not edit.
#pragma once
#include <cstdint>
#include <cstddef>

namespace runeharbor::graphics::shaders {
"""
    
    for input_file in args.inputs:
        base_name = os.path.basename(input_file)
        name = base_name.replace('.', '_') # world_vert instead of world.vert
        
        data, format_type = compile_glsl(input_file)
            
        # Convert data to C array
        hex_array = ", ".join([f"0x{b:02x}" for b in data]) if data else "0x00"
        
        header_content += f"    // From {input_file} (Format: {format_type})\n"
        header_content += f"    constexpr uint8_t {name}_data[] = {{ {hex_array} }};\n"
        header_content += f"    constexpr size_t {name}_size = sizeof({name}_data);\n"
        header_content += f"    constexpr bool {name}_is_compiled = {'true' if format_type == 'SPIRV_COMPILED' else 'false'};\n\n"
        
    header_content += "}\n"
    
    with open(args.output, "w") as f:
        f.write(header_content)

if __name__ == '__main__':
    main()
