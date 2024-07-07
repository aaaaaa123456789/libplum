#!/usr/bin/awk -f

BEGIN {
	FS = "\"" # Split lines on double quotes.

	include_at_eof = 0
	for (i = 1; i < ARGC; ++i) {
		include(ARGV[i])
	}

	ARGC = 0 # Don't attempt to process any of the files the normal way.
}

function include(path) {
	files[path] = 1

	# If the file right before this one ended with an include statement, separate both by a newline.
	# (If we are being included anywhere else, this will be false, too.)
	# Note that this *is* a global variable, modified by nested calls to `include`!
	if (include_at_eof) {
		print ""
	}

	for (; (ret = getline < path); ) {
		if (ret != 1) { # Read error?
			exit 2
		}

		if (/^\s*#\s*include\s*".+"/) {
			if (!($2 in files)) {
				include_dir = path
				if (!sub(/\/[^/]*$/, "/", include_dir)) {
					include_dir = "./"
				}
				include(include_dir $2)
			}
			include_at_eof = 1
		} else {
			print
			include_at_eof = 0 # About to process a new line.
		}
	}
}
