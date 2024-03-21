# compress html, css, js files of arg1 directory and save them in arg2 directory
# Usage: python compress.py <source_dir> <dest_dir>
# Example: python compress.py ./src ./dist

import os
import sys
import re
import htmlmin
import csscompressor
import jsmin
import shutil

def compress_html(file, dest_file):
    with open(file, 'r') as f:
        content = f.read()
    content = htmlmin.minify(content, remove_comments=True, remove_empty_space=True)
    with open(dest_file, 'w') as f:
        f.write(content)
        
    
def compress_css(file, dest_file):
    with open(file, 'r') as f:
        content = f.read()
    content = csscompressor.compress(content)
    with open(dest_file, 'w') as f:
        f.write(content)
        
def compress_js(file, dest_file):
    with open(file, 'r') as f:
        content = f.read()
    content = jsmin.jsmin(content)
    with open(dest_file, 'w') as f:
        f.write(content)
        
def compress_files(source_dir, dest_dir):
    for root, dirs, files in os.walk(source_dir):
        for file in files:
            if file.endswith('.html'):
                compress_html(os.path.join(root, file), os.path.join(dest_dir, os.path.relpath(root, source_dir), file))
            elif file.endswith('.css'):
                compress_css(os.path.join(root, file), os.path.join(dest_dir, os.path.relpath(root, source_dir), file))
            elif file.endswith('.js'):
                compress_js(os.path.join(root, file), os.path.join(dest_dir, os.path.relpath(root, source_dir), file))
            else:
                # just copy other files
                shutil.copy(os.path.join(root, file), os.path.join(dest_dir, os.path.relpath(root, source_dir), file))
                    

def get_folder_size(folder):
    total_size = 0
    for dirpath, dirnames, filenames in os.walk(folder):
        for f in filenames:
            fp = os.path.join(dirpath, f)
            total_size += os.path.getsize(fp)
    return total_size

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print('Usage: python compress.py <source_dir> <dest_dir>')
        sys.exit(1)
    source_dir = sys.argv[1]
    dest_dir = sys.argv[2]
    if os.path.exists(dest_dir):
        shutil.rmtree(dest_dir)
    os.makedirs(dest_dir)    
        
    compress_files(source_dir, dest_dir)
    
    src_size = get_folder_size(source_dir)
    dest_size = get_folder_size(dest_dir)
    
    print('Compress files from {} bytes to {} bytes (gain {:.2f}%)'.format(src_size, dest_size, (src_size - dest_size) / src_size * 100))