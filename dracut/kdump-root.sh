#!/bin/sh
# we don't mount root in kdump
# prevent dracut from failing because it does not
# understand root=kdump
rootok=1
