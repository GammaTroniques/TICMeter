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
                print(os.path.join(root, file))
                compress_html(os.path.join(root, file), os.path.join(dest_dir, os.path.relpath(root, source_dir), file))
            elif file.endswith('.css'):
                compress_css(os.path.join(root, file), os.path.join(dest_dir, os.path.relpath(root, source_dir), file))
            elif file.endswith('.js'):
                compress_js(os.path.join(root, file), os.path.join(dest_dir, os.path.relpath(root, source_dir), file))
            else:
                # just copy other files
                shutil.copy(os.path.join(root, file), os.path.join(dest_dir, os.path.relpath(root, source_dir), file))
                    
                    
if __name__ == '__main__':
    if len(sys.argv) != 3:
        print('Usage: python compress.py <source_dir> <dest_dir>')
        sys.exit(1)
    source_dir = sys.argv[1]
    dest_dir = sys.argv[2]
    if os.path.exists(source_dir):
        # clear dest_dir
        for root, dirs, files in os.walk(dest_dir):
            for file in files:
                os.remove(os.path.join(root, file))
            for dir in dirs:
                os.rmdir(os.path.join(root, dir))
    else:
        os.makedirs(dest_dir)
        
        
    compress_files(source_dir, dest_dir)
    print('Compress files successfully')
