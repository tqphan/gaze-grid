import bz2
import hashlib
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import zlib

spec=importlib.util.spec_from_file_location("trainer",Path(__file__).resolve().parents[1]/"tools/train_tatoeba.py")
trainer=importlib.util.module_from_spec(spec);spec.loader.exec_module(trainer)

class TrainingTests(unittest.TestCase):
    def test_unicode_and_character_marks(self):
        self.assertEqual(trainer.tokens("Caf\u00e9 l\u2019ami"),["caf\u00e9","l'ami"])
        self.assertEqual(trainer.tokens("\u0e01\u0e34\u0e19","tha"),["\u0e01\u0e34","\u0e19"])

    def test_turkish_casing(self):
        self.assertEqual(trainer.tokens("ISLAK \u0130STANBUL", "tur"),["\u0131slak", "istanbul"])
        self.assertEqual(trainer.tokens("I\u0307STANBUL", "tur"),["istanbul"])

    def test_reproducible_and_heldout_excluded(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);out=root/"models";out.mkdir()
            sentences=["hola mundo "+word for word in ["uno","dos","tres","cuatro","cinco","seis","siete","ocho","nueve","diez","once","doce"]]
            sentences += ["hola mundo palabra"+chr(97+i//26)+chr(97+i%26) for i in range(100)]
            sentences+=sentences
            rows=[f"{i}\tspa\t{text}\n" for i,text in enumerate(sentences)]
            (root/"spa.tsv.bz2").write_bytes(bz2.compress("".join(rows).encode()))
            first=trainer.train("spa",root,out,1000);data=(out/"spa-v1.bin").read_bytes()
            second=trainer.train("spa",root,out,1000)
            self.assertEqual(first["model_sha256"],second["model_sha256"])
            expected=sum(int.from_bytes(hashlib.sha256(text.encode()).digest()[:4],"big")%20!=0 for text in set(sentences))
            self.assertGreater(first["heldout_sentences"],0)
            self.assertEqual(first["training_sentences"],expected)
            raw=zlib.decompress(data[4:]);self.assertEqual(len(raw),struct.unpack(">I",data[:4])[0])
            self.assertTrue(raw.startswith(b"gaze-grid-prediction-v1\n"))
            self.assertIn(f"b\thola\tmundo\t{expected}\n".encode(),raw)

if __name__=="__main__": unittest.main()
