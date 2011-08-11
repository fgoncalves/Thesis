import os
import argparse


def ensure_dir(paths):
    if paths:
        try:
            if not os.path.exists(paths[0]):
                os.makedirs(paths[0])
        except OSError:
            print 'Unable to create directory ' + paths[0]
            return False
        if not paths[0][-1] == '/':
            return paths[0] + '/'
        else:
            return paths[0]

    return './'

def create_c(f, path):
    stream = open(path + f + '.c', 'w')
    stream.write('#include "' + f + '.h"\n')
    stream.close()

def create_hdr(f, path):
    stream = open(path + f + '.h', 'w')
    stream.write('#ifndef __' + f.upper() + '_H__\n')
    stream.write('#define __' + f.upper() + '_H__\n\n\n')
    stream.write('#endif\n')
    stream.close()
    

if __name__ == '__main__':

    parser = argparse.ArgumentParser(description='Create header and C files.')
    parser.add_argument('-f', '--file', metavar='NAME', type=str, nargs=1, help='File name.', required=True)
    parser.add_argument('-p', '--path', metavar='PATH', type=str, nargs=1, help='Path for files')
    parser.add_argument('-c', '--omit-c', default=False, action='store_true', help='If only the header file is required.')
    parser.add_argument('-hdr', '--omit-h', default=False, action='store_true', help='If only the c file is required.')

    args = parser.parse_args()

    path = ensure_dir(args.path)
    if path:
        if not args.omit_c:
            create_c(args.file[0], path)
        if not args.omit_h:
            create_hdr(args.file[0], path)

