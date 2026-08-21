test_env = [
  'G_TEST_SRCDIR=@0@'.format(meson.current_source_dir()),
  'G_TEST_BUILDDIR=@0@'.format(meson.current_build_dir()),
  'G_DEBUG=gc-friendly',
  'GSETTINGS_BACKEND=memory',
  'MALLOC_CHECK_=2',
]

testsuite_c_args = [
  '-DG_LOG_DOMAIN="ptyxis"',
  '-DDISABLE_AGENT_ENTRY_POINT',
  '-DG_ENABLE_DEBUG',
  '-UG_DISABLE_ASSERT',
  '-UG_DISABLE_CAST_CHECKS',
]

testsuite = {
  'test-podman' : {},
}

testsuite_deps = [
  ptyxis_agent_deps,
]

foreach test, params: testsuite
  test_exe = executable(test,
    ['@0@.c'.format(test)] + ptyxis_agent_sources,
                 c_args: testsuite_c_args,
           dependencies: testsuite_deps,
    include_directories: [include_directories('..'), include_directories('.'), include_directories('../agent')],
  )
  if not params.get('skip', false)
    test(test, test_exe, env: test_env)
  endif
endforeach

# Session-bus reproduction of the GNOME dock-badge leak. Not part of the
# default suite: it needs a running GNOME Shell. Run with:
#   python3 testsuite/test-notification-badge.py
python3 = find_program('python3', required: false)
if python3.found()
  test('notification-badge',
    python3,
    args: files('test-notification-badge.py'),
    suite: ['gnome-session'],
    is_parallel: false,
  )
endif
