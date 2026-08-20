#!/usr/bin/env bash
# Remove everything the 2026-08-20 load test created (50 devices + 2 smoke-test).
# Run on the server, from ~/sensor_board.
#
# Optional: with POLICY_SPRINT_PERSISTENT_DEVICES=false these expire on their
# own 48 h after their last reading. This just does it now.
set -eu
ADMIN="${ADMIN:-.venv/bin/python -m app.admin}"

$ADMIN delete-device loadtest-20260820-01
$ADMIN delete-device loadtest-20260820-02
$ADMIN delete-device loadtest-20260820-03
$ADMIN delete-device loadtest-20260820-04
$ADMIN delete-device loadtest-20260820-05
$ADMIN delete-device loadtest-20260820-06
$ADMIN delete-device loadtest-20260820-07
$ADMIN delete-device loadtest-20260820-08
$ADMIN delete-device loadtest-20260820-09
$ADMIN delete-device loadtest-20260820-10
$ADMIN delete-device loadtest-20260820-11
$ADMIN delete-device loadtest-20260820-12
$ADMIN delete-device loadtest-20260820-13
$ADMIN delete-device loadtest-20260820-14
$ADMIN delete-device loadtest-20260820-15
$ADMIN delete-device loadtest-20260820-16
$ADMIN delete-device loadtest-20260820-17
$ADMIN delete-device loadtest-20260820-18
$ADMIN delete-device loadtest-20260820-19
$ADMIN delete-device loadtest-20260820-20
$ADMIN delete-device loadtest-20260820-21
$ADMIN delete-device loadtest-20260820-22
$ADMIN delete-device loadtest-20260820-23
$ADMIN delete-device loadtest-20260820-24
$ADMIN delete-device loadtest-20260820-25
$ADMIN delete-device loadtest-20260820-26
$ADMIN delete-device loadtest-20260820-27
$ADMIN delete-device loadtest-20260820-28
$ADMIN delete-device loadtest-20260820-29
$ADMIN delete-device loadtest-20260820-30
$ADMIN delete-device loadtest-20260820-31
$ADMIN delete-device loadtest-20260820-32
$ADMIN delete-device loadtest-20260820-33
$ADMIN delete-device loadtest-20260820-34
$ADMIN delete-device loadtest-20260820-35
$ADMIN delete-device loadtest-20260820-36
$ADMIN delete-device loadtest-20260820-37
$ADMIN delete-device loadtest-20260820-38
$ADMIN delete-device loadtest-20260820-39
$ADMIN delete-device loadtest-20260820-40
$ADMIN delete-device loadtest-20260820-41
$ADMIN delete-device loadtest-20260820-42
$ADMIN delete-device loadtest-20260820-43
$ADMIN delete-device loadtest-20260820-44
$ADMIN delete-device loadtest-20260820-45
$ADMIN delete-device loadtest-20260820-46
$ADMIN delete-device loadtest-20260820-47
$ADMIN delete-device loadtest-20260820-48
$ADMIN delete-device loadtest-20260820-49
$ADMIN delete-device loadtest-20260820-50
$ADMIN delete-device smoketest-20260820-01
$ADMIN delete-device smoketest-20260820-02

echo 'Load-test devices removed.'
$ADMIN status
