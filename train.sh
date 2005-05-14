#! /bin/sh

# For loosening error tolerances.  Inspect the differences first!
# Assumes that the file out.test.* has been generated already
# Run this script from the application directory

target=$1
../trainerrfile.pl out.test.$target.tmp out.sample.$target.tmp out.errtol.$target
../diffn3.pl out.test.$target.tmp out.sample.$target.tmp out.errtol.$target