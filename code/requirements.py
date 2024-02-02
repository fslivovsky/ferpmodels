#!/usr/bin/env python2

import requests
import os, errno, tarfile, subprocess


def assure_dir(path):
  try:
    if not os.path.exists(path):
      os.makedirs(path)
  except OSError as e:
    if e.errno != errno.EEXIST:
      raise e


def get_tar(dir_name, url):
  tar_path = "tars/%s.tar.gz" % dir_name
  if not os.path.exists(dir_name):
    if not os.path.exists(tar_path):
      res = requests.get(url)
      with open(tar_path, "wb") as file:
        file.write(res.content)
        file.close()
    with tarfile.open(tar_path) as file:
      try:
        file.getmember(dir_name)
        file.extractall(".")
      except KeyError:
        file.extractall(dir_name)
      file.close()

  if not os.path.exists(dir_name):
    raise OSError("Could not create %s" % dir_name)


def compile_jku(dir_name, tool_name, cfg_name, cfg_command):
  tool_path = "%s/%s" % (dir_name, tool_name)
  if not os.path.exists(tool_path):
    os.chdir(dir_name)
    if cfg_name is not None and cfg_command is not None:
      os.chmod("./%s" % cfg_name, 0o0700)
      subprocess.call(cfg_command, shell=True)
    subprocess.call("make")
    os.chdir("..")

  if not os.path.exists(tool_path):
    raise OSError("Could not create %s" % tool_path)


def get_repository(dir_name, url):
  if not os.path.exists(dir_name):
    subprocess.call(["git", "clone", url])
  if not os.path.exists(dir_name):
    raise OSError("Could not create %s" % dir_name)


def compile_ijtihad():
  tool_path = "ijtihad/ijtihad"
  if not os.path.exists(tool_path):
    os.chdir("ijtihad")
    subprocess.call(["make", "clean"])
    subprocess.call(["make"])
    os.rename("mysolver", "ijtihad")
    subprocess.call(["make", "clean"])
    os.chdir("..")

  if not os.path.exists(tool_path):
    raise OSError("Could not create %s" % tool_path)


def compile_ferp_tool(dir_name, tool_name):
  tool_path = "%s/%s" % (dir_name, tool_name)
  if not os.path.exists(tool_path):
    os.chdir(dir_name)
    assure_dir("cmake-build-debug")
    os.chdir("cmake-build-debug")
    subprocess.call(["cmake", ".."])
    subprocess.call(["make", "-j"])
    os.rename(tool_name, "../%s" % tool_name)
    subprocess.call(["make", "clean"])
    os.chdir("../..")

  if not os.path.exists(tool_path):
    raise OSError("Could not create %s" % tool_path)


def main():
  assure_dir("tars")
  get_tar("booleforce-1.2", "http://fmv.jku.at/booleforce/booleforce-1.2.tar.gz")
  compile_jku("booleforce-1.2", "tracecheck", "configure", "CFLAGS='-Wall' ./configure")
  get_tar("picosat-965", "http://fmv.jku.at/picosat/picosat-965.tar.gz")
  compile_jku("picosat-965", "picosat", "configure.sh", "./configure.sh --trace")
  get_tar("certcheck-1.0.1", "http://fmv.jku.at/certcheck/certcheck-1.0.1.tar.gz")
  subprocess.call(["sed", "-i", "s/check_quantifier_levels (aig);/\/\/ check_quantifier_levels (aig);/g", "certcheck-1.0.1/certcheck.c"])
  compile_jku("certcheck-1.0.1", "certcheck", None, None)
  
  get_repository("ijtihad", "https://extgit.iaik.tugraz.at/scos/ijtihad.git")
  get_repository("toferp", "https://extgit.iaik.tugraz.at/scos/scos.sources/toferp.git")
  get_repository("ferpcert", "https://extgit.iaik.tugraz.at/scos/scos.sources/ferpcert.git")

  compile_ijtihad()
  compile_ferp_tool("toferp", "toferp")
  compile_ferp_tool("ferpcert", "ferpcheck")
  compile_jku("ferpcert2", "ferpcert", None, None)
  

if __name__ == '__main__':
  main()
