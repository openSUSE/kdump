#! /usr/bin/python3

import sys
import os
import subprocess
from pathlib import Path
import shutil
import tempfile

subdir = 'calibrate'


class PipeSource:
	def __init__(self, *args, **kwargs):
		r, w = os.pipe()
		self.out = os.fdopen(r, 'r')
		sink = os.fdopen(w, 'w')
		kwargs['stdout'] = sink
		self.process = subprocess.Popen(*args, **kwargs)
		sink.close()

project = Path('.osc/_project').read_text().strip()
package = Path('.osc/_package').read_text().strip()

def extract(repo, repoarch, name):
	outfile = tempfile.TemporaryFile("w+t")
	args = (
		'osc', 'api',
		'/'.join(('', 'build', project, repo, repoarch,
				  package, name))
	)
	oscpipe = PipeSource(args)

	args = ('rpm2cpio', '-')
	rpmpipe = PipeSource(args, stdin=oscpipe.out)

	args = (
		'cpio',
		'-i',
		'--to-stdout',
		'./usr/lib/kdump/calibrate.conf',
	)
	cpio = subprocess.Popen(args, stdin=rpmpipe.out, stdout=outfile)

	oscpipe.process.wait()
	rpmpipe.process.wait()
	cpio.wait()
		
	outfile.seek(0)
	lines = outfile.readlines()
	# if 'GENERATED_ON' value is present, use it as a prefix; 
	prefix=None
	for line in lines:
		var = line.split('=')[0]
		value = line.split('=')[1].rstrip()
		if var == "GENERATED_ON":
			prefix = value
			break

	# not a generated calibrate.conf file, ignore
	if not prefix:
		return
	
	# filter out lines with prefix from calibrate.conf.all
	calibrate = open("calibrate.conf.all", mode="r")
	oldlines = calibrate.readlines()
	calibrate.close()

	newlines=list()
	for oldline in oldlines:
		oldprefix = oldline.split(":")[0]
		if oldprefix != prefix:
			newlines.append(oldline)
		


	# append lines to calibrate.conf.all
	for line in lines:
		var = line.split('=')[0]
		value = line.split('=')[1].rstrip()
		if var == 'GENERATED_ON':
			continue
		newlines.append("{}:{}".format(prefix, line))
		print("updating {}:{}".format(prefix, line.rstrip()), file=sys.stderr)
	
	newlines.sort()
	
	calibrate = open("calibrate.conf.all", mode="w")
	calibrate.writelines(newlines)
	calibrate.close()


def list_rpms():
	files = list()
	args = (
		'osc', 'ls',
		'-b',					# Binaries
	)
	oscpipe = PipeSource(args)
	repoarch = None
	for line in oscpipe.out:
		if not line.startswith(' '):
			repoarch = line.split('/')[1].rstrip()
			repo = line.split('/')[0].rstrip()
		else:
			filename = line.strip()
			try:
				nvr, arch, line = filename.rsplit('.', 2)
				name, ver, rel = filename.rsplit('-', 2)
			except ValueError:
				name = None
			if name == 'kdump' and arch != 'src':
				files.append((repo, repoarch, filename))
	return files


shutil.copyfile("calibrate.conf.all", "calibrate.conf.all.old")
print("calibrate.conf.all saved as calibrate.conf.all.old")

for repo, arch, name in list_rpms():
	print('extractng calibrate.conf from {}'.format(name), file=sys.stderr)
	extract(repo, arch, name)
	print(file=sys.stderr)


print('calibrate.conf.all updated', file=sys.stderr)
