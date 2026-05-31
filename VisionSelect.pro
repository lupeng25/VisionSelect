TEMPLATE = subdirs

SUBDIRS += app license_key_generator tests
license_key_generator.subdir = tools/license_key_generator
tests.depends = app
