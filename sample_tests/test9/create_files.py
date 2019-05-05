for i in xrange(1, 10000):
    a = str(i).zfill(4)
    with open('File{}.txt'.format(a), 'w') as f:
        f.write('This is file {}.'.format(a))
