# tests that examples provided with Graphviz compile correctly

import os
import platform
import pytest
import shutil
import subprocess

def c_compiler():
    '''find the system's C compiler'''
    if platform.system() == 'Windows':
      return "cl"
    else:
      return os.environ.get('CC', 'cc')

@pytest.mark.parametrize('src', ['demo.c', 'dot.c', 'example.c', 'neatopack.c',
  'simple.c'])
# FIXME: Remove skip when
# https://gitlab.com/graphviz/graphviz/-/issues/1777 is fixed
@pytest.mark.skipif(
    os.getenv('build_system') == 'msbuild',
    reason='Windows MSBuild release does not contain any header files (#1777)'
)
def test_compile_example(src):
    '''try to compile the example'''

    # construct an absolute path to the example
    filepath = os.path.join(os.path.abspath(os.path.dirname(__file__)),
      '..', 'dot.demo', src)

    cc = c_compiler()

    libs = ('cgraph', 'gvc')

    # ensure the C compiler can build this without error
    if platform.system() == 'Windows':
      subprocess.check_call([cc, filepath, '-nologo', '-link']
        + ['{}.lib'.format(l) for l in libs])
    else:
      subprocess.check_call([cc, '-o', os.devnull, filepath]
        + ['-l{}'.format(l) for l in libs])

@pytest.mark.parametrize('src', ['addrings', 'attr', 'bbox', 'bipart',
  'chkedges', 'clustg', 'collapse', 'cycle', 'deghist', 'delmulti', 'depath',
  'flatten', 'group', 'indent', 'path', 'scale', 'span', 'treetoclust',
  'addranks', 'anon', 'bb', 'chkclusters', 'cliptree', 'col', 'color',
  'dechain', 'deledges', 'delnodes', 'dijkstra', 'get-layers-list', 'knbhd',
  'maxdeg', 'rotate', 'scalexy', 'topon'])
def test_gvpr_example(src):
    '''check GVPR can parse the given example'''

    # skip this test if GVPR is unavailable
    if shutil.which('gvpr') is None:
      pytest.skip('GVPR not available')

# FIXME: remove when https://gitlab.com/graphviz/graphviz/-/issues/1784 is fixed
    if os.environ.get('build_system') == 'msbuild' and \
      os.environ.get('configuration') == 'Debug' and \
      src in ['bbox', 'col']:
      pytest.skip('GVPR tests "bbox" and "col" hangs on Windows MSBuild Debug '
                  'builds (#1784)')

    # construct an absolute path to the example
    path = os.path.join(os.path.abspath(os.path.dirname(__file__)),
      '../cmd/gvpr/lib', src)

    # run GVPR with the given script
    with open(os.devnull, 'rt') as nul:
      subprocess.check_call(['gvpr', '-f', path], stdin=nul)

@pytest.mark.skipif(shutil.which('gvpr') is None, reason='GVPR not available')
def test_gvpr_clustg():
    '''check cmd/gvpr/lib/clustg works'''

    # construct an absolute path to clustg
    path = os.path.join(os.path.abspath(os.path.dirname(__file__)),
      '../cmd/gvpr/lib/clustg')

    # some sample input
    input = 'digraph { N1; N2; N1 -> N2; N3; }'

    # run GVPR on this input
    p = subprocess.Popen(['gvpr', '-f', path], stdin=subprocess.PIPE,
      stdout=subprocess.PIPE, universal_newlines=True)
    output, _ = p.communicate(input)

    assert p.returncode == 0, 'GVPR exited with non-zero status'

    assert output.strip() == 'strict digraph "clust%1" {\n' \
                             '\tnode [_cnt=0];\n' \
                             '\tedge [_cnt=0];\n' \
                             '\tN1 -> N2\t[_cnt=1];\n' \
                             '\tN3;\n' \
                             '}', 'unexpected output'
