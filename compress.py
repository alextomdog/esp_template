import sys
import os

# usage: python compress.py <input_file> <output_file>


def compress(input_file, output_file):
    output_file_content = []
    with open(input_file, 'r', encoding="utf-8") as f:
        is_using_multiple_comments = False
        for line in f:
            line = line.strip()  # remove leading and trailing whitespace

            # Remove single-line comments (//)
            if line.startswith('//'):
                continue

            # Remove inline comments (//)
            if '//' in line:
                line = line.split('//')[0]

            # Handle multi-line comments (/*...*/)
            if '/*' in line:
                is_using_multiple_comments = True
                line = line.split('/*')[0]  # remove everything after /*

            if '*/' in line:
                is_using_multiple_comments = False
                line = line.split('*/')[1]  # remove everything before */

            if is_using_multiple_comments:
                continue  # Skip lines inside multi-line comments

            if line:  # Only add non-empty lines
                output_file_content.append(line)

    # Write the compressed content to the output file
    with open(output_file, 'w', encoding="utf-8") as f:
        # Join the lines with newline characters
        f.write("\n".join(output_file_content))


if __name__ == "__main__":
    input_file = sys.argv[1]

    try:
        output_file = sys.argv[2]
    except IndexError:
        # Default to output filename based on input file
        file_basename = os.path.basename(input_file)
        file_directory = os.path.dirname(input_file)
        output_filename = file_basename.split(
            ".")[0] + "_compressed." + file_basename.split(".")[1]
        output_file = os.path.join(file_directory, output_filename)
        print(f"Output file not specified, using default: {output_file}")

    try:
        compress(input_file, output_file)
        print(f"Compressed {input_file} to {output_file}")
    except Exception as e:
        print(f"Error: {e}")
