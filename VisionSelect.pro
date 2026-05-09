TEMPLATE = subdirs

SUBDIRS += app tests
tests.depends = app
