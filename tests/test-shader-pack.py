"""Exercise the pinned installer against valid and adversarial data archives."""
from pathlib import Path
import hashlib, importlib.util, io, json, tempfile, unittest, zipfile
from unittest.mock import patch

repo=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('shader_installer',repo/'runtime/download-shader-pack.py')
installer=importlib.util.module_from_spec(spec);spec.loader.exec_module(installer)

class InstallerTests(unittest.TestCase):
    def archive(self,name='glsl/reyondpack/test.glsl'):
        data=b'// SPDX-License-Identifier: MIT\n';buffer=io.BytesIO()
        with zipfile.ZipFile(buffer,'w',zipfile.ZIP_DEFLATED) as z:z.writestr(name,data)
        result=buffer.getvalue()
        return result,{'url':'https://github.com/reyond9079/Reyond-FTE-Assets/releases/download/test/pack.zip','version':'test','size':len(result),'sha256':hashlib.sha256(result).hexdigest(),'files':{name:{'size':len(data),'sha256':hashlib.sha256(data).hexdigest()}}}
    def run_case(self,data,manifest):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);(root/'manifest.json').write_text(json.dumps(manifest))
            with patch.object(installer.urllib.request,'urlopen',return_value=io.BytesIO(data)):
                installer.install(root/'install',root/'manifest.json')
            self.assertTrue((root/'install/ready.txt').exists())
    def test_valid(self):self.run_case(*self.archive())
    def test_bad_checksum(self):
        data,manifest=self.archive();manifest['sha256']='0'*64
        with self.assertRaisesRegex(ValueError,'checksum'):self.run_case(data,manifest)
    def test_traversal(self):
        with self.assertRaisesRegex(ValueError,'Unsafe'):self.run_case(*self.archive('../outside.glsl'))
    def test_bad_file_checksum(self):
        data,manifest=self.archive();next(iter(manifest['files'].values()))['sha256']='0'*64
        with self.assertRaisesRegex(ValueError,'checksum'):self.run_case(data,manifest)
    def test_unexpected_entries(self):
        data,manifest=self.archive();manifest['files']={}
        with self.assertRaisesRegex(ValueError,'Unexpected'):self.run_case(data,manifest)

if __name__=='__main__':unittest.main()
