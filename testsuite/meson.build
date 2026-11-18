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

command_plan_test = executable('test-command-plan', 'test-command-plan.c',
       dependencies: ptyxis_deps,
         link_whole: ptyxis_core,
             c_args: testsuite_c_args,
  include_directories: ptyxis_include_directories,
)
test('test-command-plan', command_plan_test, env: test_env)

early_options_test = executable('test-early-options',
  ['test-early-options.c', '../src/ptyxis-early-options.c'],
       dependencies: dependency('gio-2.0', version: glib_req),
             c_args: testsuite_c_args,
  include_directories: [include_directories('..'), include_directories('../src')],
)
test('test-early-options', early_options_test, env: test_env)
