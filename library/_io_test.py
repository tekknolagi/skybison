#!/usr/bin/env python3
# Copyright (c) Facebook, Inc. and its affiliates. (http://www.facebook.com)
import _io
import array
import mmap
import os
import subprocess
import sys
import tempfile
import unittest
import unittest.mock
from unittest.mock import Mock

from test_support import pyro_only


try:
    from _io import _BufferedIOMixin
except ImportError:
    pass


def _getfd():
    r, w = os.pipe()
    os.close(w)  # So that read() is harmless
    return r


class _IOBaseTests(unittest.TestCase):
    def test_closed_with_closed_instance_returns_true(self):
        f = _io._IOBase()
        self.assertFalse(f.closed)
        f.close()
        self.assertTrue(f.closed)

    def test_fileno_raises_unsupported_operation(self):
        f = _io._IOBase()
        self.assertRaises(_io.UnsupportedOperation, f.fileno)

    def test_flush_with_closed_instance_raises_value_error(self):
        f = _io._IOBase()
        f.close()
        with self.assertRaises(ValueError):
            f.flush()

    def test_isatty_with_closed_instance_raises_value_error(self):
        f = _io._IOBase()
        f.close()
        with self.assertRaises(ValueError):
            f.isatty()

    def test_isatty_always_returns_false(self):
        f = _io._IOBase()
        self.assertEqual(f.isatty(), False)

    def test_iter_with_closed_instance_raises_value_error(self):
        f = _io._IOBase()
        f.close()
        with self.assertRaises(ValueError):
            f.__iter__()

    def test_next_reads_line(self):
        class C(_io._IOBase):
            def readline(self):
                return "foo"

        f = C()
        self.assertEqual(f.__next__(), "foo")

    def test_readline_calls_read(self):
        class C(_io._IOBase):
            def read(self, size):
                raise UserWarning("foo")

        f = C()
        with self.assertRaises(UserWarning):
            f.readline()

    def test_readlines_calls_read(self):
        class C(_io._IOBase):
            def read(self, size):
                raise UserWarning("foo")

        f = C()
        with self.assertRaises(UserWarning):
            f.readlines()

    def test_readlines_calls_readline(self):
        class C(_io._IOBase):
            def readline(self):
                raise UserWarning("foo")

        f = C()
        with self.assertRaises(UserWarning):
            f.readlines()

    def test_readlines_returns_list(self):
        class C(_io._IOBase):
            def __iter__(self):
                return ["x", "y", "z"].__iter__()

        f = C()
        self.assertEqual(f.readlines(), ["x", "y", "z"])

    def test_readable_always_returns_false(self):
        f = _io._IOBase()
        self.assertFalse(f.readable())

    def test_seek_raises_unsupported_operation(self):
        f = _io._IOBase()
        self.assertRaises(_io.UnsupportedOperation, f.seek, 0)

    def test_seekable_always_returns_false(self):
        f = _io._IOBase()
        self.assertFalse(f.seekable())

    def test_supports_arbitrary_attributes(self):
        f = _io._IOBase()
        f.hello_world = 42
        self.assertEqual(f.hello_world, 42)

    def test_tell_raises_unsupported_operation(self):
        f = _io._IOBase()
        self.assertRaises(_io.UnsupportedOperation, f.tell)

    def test_truncate_raises_unsupported_operation(self):
        f = _io._IOBase()
        self.assertRaises(_io.UnsupportedOperation, f.truncate)

    def test_writable_always_returns_false(self):
        f = _io._IOBase()
        self.assertFalse(f.writable())

    def test_with_with_closed_instance_raises_value_error(self):
        f = _io._IOBase()
        f.close()
        with self.assertRaises(ValueError):
            with f:
                pass

    def test_writelines_calls_write(self):
        class C(_io._IOBase):
            def write(self, line):
                raise UserWarning("foo")

        f = C()
        with self.assertRaises(UserWarning):
            f.writelines(["a"])


class _RawIOBaseTests(unittest.TestCase):
    def test_read_with_readinto_returning_none_returns_none(self):
        class C(_io._RawIOBase):
            def readinto(self, b):
                return None

        f = C()
        self.assertEqual(f.read(), None)

    def test_read_with_negative_size_calls_readall(self):
        class C(_io._RawIOBase):
            def readall(self):
                raise UserWarning("foo")

        f = C()
        with self.assertRaises(UserWarning):
            f.read(-5)

    def test_read_with_nonnegative_size_calls_readinto(self):
        class C(_io._RawIOBase):
            def readinto(self, b):
                for i in range(len(b)):
                    b[i] = ord("x")
                return len(b)

        f = C()
        self.assertEqual(f.read(5), b"xxxxx")

    def test_readall_calls_read(self):
        class C(_io._RawIOBase):
            def read(self, size):
                raise UserWarning("foo")

        f = C()
        with self.assertRaises(UserWarning):
            f.readall()

    def test_readinto_raises_not_implemented_error(self):
        f = _io._RawIOBase()
        b = bytearray(b"")
        self.assertRaises(NotImplementedError, f.readinto, b)

    def test_write_raises_not_implemented_error(self):
        f = _io._RawIOBase()
        self.assertRaises(NotImplementedError, f.write, b"")


class _BufferedIOBaseTests(unittest.TestCase):
    def test_detach_raises_unsupported_operation(self):
        f = _io._BufferedIOBase()
        self.assertIn("detach", _io._BufferedIOBase.__dict__)
        self.assertRaises(_io.UnsupportedOperation, f.detach)

    def test_read_raises_unsupported_operation(self):
        f = _io._BufferedIOBase()
        self.assertIn("read", _io._BufferedIOBase.__dict__)
        self.assertRaises(_io.UnsupportedOperation, f.read)

    def test_read1_raises_unsupported_operation(self):
        f = _io._BufferedIOBase()
        self.assertIn("read1", _io._BufferedIOBase.__dict__)
        self.assertRaises(_io.UnsupportedOperation, f.read1)

    def test_readinto1_calls_read1(self):
        class C(_io._BufferedIOBase):
            def read1(self, n):
                raise UserWarning("foo")

        f = C()
        with self.assertRaises(UserWarning):
            f.readinto1(bytearray())

    def test_readinto_calls_read(self):
        class C(_io._BufferedIOBase):
            def read(self, n):
                raise UserWarning("foo")

        f = C()
        with self.assertRaises(UserWarning):
            f.readinto(bytearray())

    def test_write_raises_unsupported_operation(self):
        f = _io._BufferedIOBase()
        self.assertIn("write", _io._BufferedIOBase.__dict__)
        self.assertRaises(_io.UnsupportedOperation, f.write, b"")


class BufferedRWPairTests(unittest.TestCase):
    def test_dunder_init_with_non_readable_reader_raises_unsupported_operation(self):
        class C:
            def readable(self):
                return False

            def writable(self):
                return False

        with self.assertRaisesRegex(_io.UnsupportedOperation, "readable"):
            _io.BufferedRWPair(C(), C())

    def test_dunder_init_with_non_writable_writer_raises_unsupported_operation(self):
        class C:
            def readable(self):
                return True

            def writable(self):
                return False

        with self.assertRaisesRegex(_io.UnsupportedOperation, "writable"):
            _io.BufferedRWPair(C(), C())

    def test_close_calls_reader_close_and_writer_close(self):
        class Reader(_io.BytesIO):
            close = Mock(name="close")

        class Writer(_io.BytesIO):
            close = Mock(name="close")

        with Reader() as reader, Writer() as writer:
            with _io.BufferedRWPair(reader, writer) as buffer:
                buffer.close()
                reader.close.assert_called_once()
                writer.close.assert_called_once()

    def test_closed_returns_writer_closed(self):
        with _io.BytesIO() as reader, _io.BytesIO() as writer:
            with _io.BufferedRWPair(reader, writer) as buffer:
                self.assertFalse(buffer.closed)
                writer.close()
                self.assertTrue(buffer.closed)

    def test_flush_calls_writer_write(self):
        write_calls = 0

        class Reader(_io.BytesIO):
            pass

        class Writer(_io.BytesIO):
            def write(self, b):
                nonlocal write_calls
                write_calls += 1
                return super().write(b)

        with Reader() as reader, Writer() as writer:
            with _io.BufferedRWPair(reader, writer) as buffer:
                buffer.write(b"123")
                self.assertEqual(write_calls, 0)
                buffer.flush()
                self.assertEqual(write_calls, 1)

    def test_isatty_with_tty_reader_returns_true(self):
        class Reader(_io.BytesIO):
            isatty = Mock(name="isatty", return_value=True)

        class Writer(_io.BytesIO):
            isatty = Mock(name="isatty", return_value=False)

        with Reader() as reader, Writer() as writer:
            with _io.BufferedRWPair(reader, writer) as buffer:
                self.assertTrue(buffer.isatty())
                reader.isatty.assert_called_once()
                writer.isatty.assert_called_once()

    def test_isatty_with_tty_writer_returns_true(self):
        class Reader(_io.BytesIO):
            isatty = Mock(name="isatty", return_value=False)

        class Writer(_io.BytesIO):
            isatty = Mock(name="isatty", return_value=True)

        with Reader() as reader, Writer() as writer:
            with _io.BufferedRWPair(reader, writer) as buffer:
                self.assertTrue(buffer.isatty())
                reader.isatty.assert_not_called()
                writer.isatty.assert_called_once()

    def test_isatty_with_neither_tty_returns_false(self):
        class Reader(_io.BytesIO):
            isatty = Mock(name="isatty", return_value=False)

        class Writer(_io.BytesIO):
            isatty = Mock(name="isatty", return_value=False)

        with Reader() as reader, Writer() as writer:
            with _io.BufferedRWPair(reader, writer) as buffer:
                self.assertFalse(buffer.isatty())
                reader.isatty.assert_called_once()
                writer.isatty.assert_called_once()

    def test_peek_does_not_call_reader_or_writer_peek(self):
        class Reader(_io.BytesIO):
            peek = Mock(name="peek")

        class Writer(_io.BytesIO):
            peek = Mock(name="peek")

        with Reader() as reader, Writer() as writer:
            with _io.BufferedRWPair(reader, writer) as buffer:
                buffer.peek()
                reader.peek.assert_not_called()
                writer.peek.assert_not_called()

    def test_read_calls_reader_readall(self):
        class Reader(_io.BytesIO):
            read = Mock(name="read")
            readall = Mock(name="readall", return_value=None)

        with Reader() as reader, _io.BytesIO() as writer:
            with _io.BufferedRWPair(reader, writer) as buffer:
                buffer.read()
                reader.read.assert_not_called()
                reader.readall.assert_called_once()

    def test_readable_on_closed_raises_value_error(self):
        with _io.BytesIO() as reader, _io.BytesIO() as writer:
            with _io.BufferedRWPair(reader, writer) as buffer:
                pass
        with self.assertRaises(ValueError) as context:
            buffer.readable()
        self.assertIn("closed file", str(context.exception))

    def test_readable_calls_reader_readable(self):
        class Reader(_io.BytesIO):
            readable = Mock(name="readable", return_value=True)

        class Writer(_io.BytesIO):
            readable = Mock(name="readable")

        with Reader() as reader, Writer() as writer:
            with _io.BufferedRWPair(reader, writer) as buffer:
                self.assertEqual(reader.readable.call_count, 2)
                self.assertTrue(buffer.readable())
                self.assertEqual(reader.readable.call_count, 3)
                writer.readable.assert_not_called()

    def test_writable_on_closed_raises_value_error(self):
        with _io.BytesIO() as reader, _io.BytesIO() as writer:
            with _io.BufferedRWPair(reader, writer) as buffer:
                pass
        with self.assertRaises(ValueError) as context:
            buffer.writable()
        self.assertIn("closed file", str(context.exception))

    def test_writable_calls_reader_writable(self):
        class Reader(_io.BytesIO):
            writable = Mock(name="writable")

        class Writer(_io.BytesIO):
            writable = Mock(name="writable", return_value=True)

        with Reader() as reader, Writer() as writer:
            with _io.BufferedRWPair(reader, writer) as buffer:
                self.assertEqual(writer.writable.call_count, 2)
                self.assertTrue(buffer.writable())
                reader.writable.assert_not_called()
                self.assertEqual(writer.writable.call_count, 3)


class BufferedRandomTests(unittest.TestCase):
    def test_dunder_init_with_non_seekable_raises_unsupported_operation(self):
        class C(_io.BytesIO):
            def seekable(self):
                return False

        c = C()
        with self.assertRaisesRegex(_io.UnsupportedOperation, "not seekable"):
            _io.BufferedRandom(c)

    def test_dunder_init_with_non_readable_raises_unsupported_operation(self):
        class C(_io.BytesIO):
            def readable(self):
                return False

        c = C()
        with self.assertRaisesRegex(_io.UnsupportedOperation, "not readable"):
            _io.BufferedRandom(c)

    def test_dunder_init_with_non_writable_raises_unsupported_operation(self):
        class C(_io.BytesIO):
            def writable(self):
                return False

        c = C()
        with self.assertRaisesRegex(_io.UnsupportedOperation, "not writable"):
            _io.BufferedRandom(c)

    def test_dunder_init_with_non_positive_buffer_size_raises_value_error(self):
        with _io.BytesIO(b"hello") as bytes_io:
            with self.assertRaisesRegex(ValueError, "buffer size"):
                _io.BufferedRandom(bytes_io, 0)

    def test_dunder_init_allows_subclasses(self):
        class C(_io.BufferedRandom):
            pass

        instance = C(_io.BytesIO(b""))
        self.assertIsInstance(instance, _io.BufferedRandom)

    def test_flush_with_closed_raises_value_error(self):
        writer = _io.BufferedRandom(_io.BytesIO(b"hello"))
        writer.close()
        self.assertRaisesRegex(ValueError, "flush of closed file", writer.flush)

    def test_flush_writes_buffered_bytes(self):
        with _io.BytesIO(b"Hello world!") as bytes_io:
            with _io.BufferedRandom(bytes_io) as writer:
                writer.write(b"foo bar")
                self.assertEqual(bytes_io.getvalue(), b"Hello world!")
                writer.flush()
                self.assertEqual(bytes_io.getvalue(), b"foo barorld!")

    def test_peek_returns_bytes(self):
        with _io.BytesIO(b"hello") as bytes_io:
            with _io.BufferedRandom(bytes_io) as buffer:
                self.assertEqual(buffer.peek(), b"hello")

    def test_peek_with_negative_returns_bytes(self):
        with _io.BytesIO(b"hello") as bytes_io:
            with _io.BufferedRandom(bytes_io) as buffer:
                self.assertEqual(buffer.peek(-1), b"hello")

    def test_raw_returns_raw(self):
        with _io.BytesIO(b"hello") as bytes_io:
            with _io.BufferedRandom(bytes_io) as buffered_io:
                self.assertIs(buffered_io.raw, bytes_io)

    def test_read_with_detached_stream_raises_value_error(self):
        with _io.BytesIO() as bytes_io:
            buffered = _io.BufferedRandom(bytes_io)
            buffered.detach()
            self.assertRaisesRegex(ValueError, "detached", buffered.read)

    def test_read_with_closed_stream_raises_value_error(self):
        bytes_io = _io.BytesIO(b"hello")
        buffered = _io.BufferedRandom(bytes_io)
        bytes_io.close()
        self.assertRaisesRegex(ValueError, "closed file", buffered.read)

    def test_read_with_negative_size_raises_value_error(self):
        with _io.BytesIO() as bytes_io:
            buffered = _io.BufferedRandom(bytes_io)
            with self.assertRaisesRegex(ValueError, "read length"):
                buffered.read(-2)

    def test_read_with_none_size_calls_raw_readall(self):
        class C(_io.BytesIO):
            def readall(self):
                raise UserWarning("foo")

        with C() as bytes_io:
            buffered = _io.BufferedRandom(bytes_io)
            with self.assertRaises(UserWarning) as context:
                buffered.read(None)
        self.assertEqual(context.exception.args, ("foo",))

    def test_read_reads_bytes(self):
        with _io.BytesIO(b"hello") as bytes_io:
            buffered = _io.BufferedRandom(bytes_io, buffer_size=1)
            result = buffered.read()
            self.assertEqual(result, b"hello")

    def test_read_reads_count_bytes(self):
        with _io.BytesIO(b"hello") as bytes_io:
            buffered = _io.BufferedRandom(bytes_io, buffer_size=1)
            result = buffered.read(3)
            self.assertEqual(result, b"hel")

    def test_read1_with_negative_size_returns_rest_in_buffer(self):
        with _io.BytesIO(b"hello world") as bytes_io:
            buffered = _io.BufferedRandom(bytes_io, buffer_size=5)
            self.assertEqual(buffered.read1(-1), b"hello")

    def test_read1_calls_read(self):
        with _io.BytesIO(b"hello") as bytes_io:
            buffered = _io.BufferedRandom(bytes_io, buffer_size=10)
            result = buffered.read1(3)
            self.assertEqual(result, b"hel")

    def test_read1_reads_from_buffer(self):
        with _io.BytesIO(b"hello") as bytes_io:
            buffered = _io.BufferedRandom(bytes_io, buffer_size=4)
            buffered.read(1)
            result = buffered.read1(10)
            self.assertEqual(result, b"ell")

    def test_readable_calls_raw_readable(self):
        class C(_io.BytesIO):
            readable = Mock(name="readable", return_value=True)

        with C() as raw:
            with _io.BufferedRandom(raw) as buffer:
                self.assertEqual(raw.readable.call_count, 1)
                self.assertTrue(buffer.readable())
                self.assertEqual(raw.readable.call_count, 2)

    def test_seek_with_invalid_whence_raises_value_error(self):
        with _io.BufferedRandom(_io.BytesIO(b"hello")) as writer:
            with self.assertRaisesRegex(ValueError, "invalid whence"):
                writer.seek(0, 3)

    def test_seek_calls_raw_seek(self):
        class C(_io.BytesIO):
            seek = Mock(name="seek", return_value=42)

        with C(b"hello") as bytes_io:
            with _io.BufferedRandom(bytes_io) as writer:
                self.assertEqual(writer.seek(10), 42)
                bytes_io.seek.assert_called_once()

    def test_seek_resets_position(self):
        with _io.BytesIO(b"hello") as bytes_io:
            buffered = _io.BufferedRandom(bytes_io)
            buffered.read(2)
            self.assertEqual(buffered.tell(), 2)
            buffered.seek(0)
            self.assertEqual(buffered.tell(), 0)

    def test_supports_arbitrary_attributes(self):
        with _io.BytesIO(b"hello") as bytes_io:
            buffered = _io.BufferedRandom(bytes_io)
            buffered.bonjour = -99
            self.assertEqual(buffered.bonjour, -99)

    def test_tell_returns_current_position(self):
        with _io.BytesIO(b"hello") as bytes_io:
            buffered = _io.BufferedRandom(bytes_io)
            self.assertEqual(buffered.tell(), 0)
            buffered.read(2)
            self.assertEqual(buffered.tell(), 2)

    def test_tell_counts_buffered_bytes(self):
        with _io.BytesIO(b"hello") as bytes_io:
            with _io.BufferedRandom(bytes_io) as writer:
                self.assertEqual(bytes_io.tell(), 0)
                self.assertEqual(writer.tell(), 0)
                writer.write(b"123")
                self.assertEqual(bytes_io.tell(), 0)
                self.assertEqual(writer.tell(), 3)

    def test_truncate_uses_tell_for_default_pos(self):
        with _io.BytesIO(b"hello") as bytes_io:
            with _io.BufferedRandom(bytes_io) as writer:
                writer.write(b"123")
                self.assertEqual(writer.truncate(), 3)
                self.assertEqual(bytes_io.getvalue(), b"123")

    def test_truncate_with_pos_truncates_raw(self):
        with _io.BytesIO(b"hello") as bytes_io:
            with _io.BufferedRandom(bytes_io) as writer:
                writer.write(b"123")
                self.assertEqual(writer.truncate(4), 4)
                self.assertEqual(bytes_io.getvalue(), b"123l")

    def test_writable_calls_raw_writable(self):
        class C(_io.BytesIO):
            writable = Mock(name="writable", return_value=True)

        with C(b"hello") as bytes_io:
            with _io.BufferedRandom(bytes_io) as writer:
                bytes_io.writable.assert_called_once()
                self.assertIs(writer.writable(), True)
                self.assertEqual(bytes_io.writable.call_count, 2)

    def test_write_with_closed_raises_value_error(self):
        writer = _io.BufferedRandom(_io.BytesIO(b"hello"))
        writer.close()
        with self.assertRaisesRegex(ValueError, "write to closed file"):
            writer.write(b"")

    def test_write_with_str_raises_type_error(self):
        with _io.BufferedRandom(_io.BytesIO(b"hello")) as writer:
            with self.assertRaisesRegex(TypeError, "str"):
                writer.write("")


class BufferedWriterTests(unittest.TestCase):
    def test_dunder_init_with_non_writable_stream_raises_os_error(self):
        with _io.FileIO(_getfd(), mode="r") as file_reader:
            with self.assertRaises(OSError) as context:
                _io.BufferedWriter(file_reader)
        self.assertEqual(str(context.exception), "File or stream is not writable.")

    def test_dunder_init_with_non_positive_size_raises_value_error(self):
        with _io.BytesIO(b"123") as bytes_writer:
            with self.assertRaises(ValueError) as context:
                _io.BufferedWriter(bytes_writer, 0)
            self.assertEqual(
                str(context.exception), "buffer size must be strictly positive"
            )

    def test_dunder_init_allows_subclasses(self):
        class C(_io.BufferedWriter):
            pass

        instance = C(_io.BytesIO(b""))
        self.assertIsInstance(instance, _io.BufferedWriter)

    def test_flush_with_closed_raises_value_error(self):
        writer = _io.BufferedWriter(_io.BytesIO(b"hello"))
        writer.close()
        with self.assertRaises(ValueError) as context:
            writer.flush()
        self.assertEqual(str(context.exception), "flush of closed file")

    def test_flush_writes_buffered_bytes(self):
        with _io.BytesIO(b"Hello world!") as bytes_io:
            with _io.BufferedWriter(bytes_io) as writer:
                writer.write(b"foo bar")
                self.assertEqual(bytes_io.getvalue(), b"Hello world!")
                writer.flush()
                self.assertEqual(bytes_io.getvalue(), b"foo barorld!")

    def test_raw_returns_raw(self):
        with _io.BytesIO(b"hello") as bytes_io:
            with _io.BufferedWriter(bytes_io) as buffered_io:
                self.assertIs(buffered_io.raw, bytes_io)

    def test_seek_with_invalid_whence_raises_value_error(self):
        with _io.BufferedWriter(_io.BytesIO(b"hello")) as writer:
            with self.assertRaisesRegex(ValueError, "invalid whence"):
                writer.seek(0, 3)

    def test_seek_calls_raw_seek(self):
        class C(_io.BytesIO):
            seek = Mock(name="seek", return_value=42)

        with C(b"hello") as bytes_io:
            with _io.BufferedWriter(bytes_io) as writer:
                self.assertEqual(writer.seek(10), 42)
                bytes_io.seek.assert_called_once()

    def test_supports_arbitrary_attributes(self):
        with _io.BytesIO(b"hello") as bytes_io:
            with _io.BufferedWriter(bytes_io) as buffered_io:
                buffered_io.guten_tag = 5.5
                self.assertEqual(buffered_io.guten_tag, 5.5)

    def test_tell_counts_buffered_bytes(self):
        with _io.BytesIO(b"hello") as bytes_io:
            with _io.BufferedWriter(bytes_io) as writer:
                self.assertEqual(bytes_io.tell(), 0)
                self.assertEqual(writer.tell(), 0)
                writer.write(b"123")
                self.assertEqual(bytes_io.tell(), 0)
                self.assertEqual(writer.tell(), 3)

    def test_truncate_uses_tell_for_default_pos(self):
        with _io.BytesIO(b"hello") as bytes_io:
            with _io.BufferedWriter(bytes_io) as writer:
                writer.write(b"123")
                self.assertEqual(writer.truncate(), 3)
                self.assertEqual(bytes_io.getvalue(), b"123")

    def test_truncate_with_pos_truncates_raw(self):
        with _io.BytesIO(b"hello") as bytes_io:
            with _io.BufferedWriter(bytes_io) as writer:
                writer.write(b"123")
                self.assertEqual(writer.truncate(4), 4)
                self.assertEqual(bytes_io.getvalue(), b"123l")

    def test_writable_calls_raw_writable(self):
        class C(_io.BytesIO):
            writable = Mock(name="writable", return_value=True)

        with C(b"hello") as bytes_io:
            with _io.BufferedWriter(bytes_io) as writer:
                bytes_io.writable.assert_called_once()
                self.assertIs(writer.writable(), True)
                self.assertEqual(bytes_io.writable.call_count, 2)

    def test_write_with_closed_raises_value_error(self):
        writer = _io.BufferedWriter(_io.BytesIO(b"hello"))
        writer.close()
        with self.assertRaises(ValueError) as context:
            writer.write(b"")
        self.assertEqual(str(context.exception), "write to closed file")

    def test_write_with_str_raises_type_error(self):
        with _io.BufferedWriter(_io.BytesIO(b"hello")) as writer:
            with self.assertRaisesRegex(TypeError, "str"):
                writer.write("")


class BytesIOTests(unittest.TestCase):
    def test_dunder_init_with_none_initial_value_sets_empty_string(self):
        with _io.BytesIO() as f:
            self.assertEqual(f.getvalue(), b"")

    def test_dunder_init_with_non_bytes_raises_type_error(self):
        self.assertRaisesRegex(
            TypeError,
            "a bytes-like object is required, not 'str'",
            _io.BytesIO,
            "not_bytes",
        )

    def test_dunder_init_with_bytes_returns_bytesio_instance(self):
        f = _io.BytesIO(b"foo")
        self.assertIsInstance(f, _io.BytesIO)
        self.assertEqual(f.getvalue(), b"foo")
        f.close()

    def test_dunder_init_with_bytes_subclass_returns_bytesio_instance(self):
        class C(bytes):
            pass

        instance = C(b"hello")
        f = _io.BytesIO(instance)
        self.assertIsInstance(f, _io.BytesIO)
        self.assertEqual(f.getvalue(), b"hello")

    def test_dunder_init_with_bytearray_subclass_returns_bytesio_instance(self):
        class C(bytearray):
            pass

        instance = C(bytearray(b"hello"))
        f = _io.BytesIO(instance)
        self.assertIsInstance(f, _io.BytesIO)
        self.assertEqual(f.getvalue(), b"hello")

    def test_dunder_init_with_bytearray_returns_bytesio_instance(self):
        bytes_array = bytearray(b"hello")
        f = _io.BytesIO(bytes_array)
        self.assertIsInstance(f, _io.BytesIO)
        self.assertEqual(f.getvalue(), b"hello")

    def test_dunder_init_with_memoryview_of_bytes_returns_bytesio_instance(self):
        mem = memoryview(b"hello")
        f = _io.BytesIO(mem)
        self.assertIsInstance(f, _io.BytesIO)
        self.assertEqual(f.getvalue(), b"hello")

    def test_dunder_init_with_memoryview_of_bytearray_returns_bytesio_instance(self):
        bytes_array = bytearray(b"hello")
        mem = memoryview(bytes_array)
        f = _io.BytesIO(mem)
        self.assertIsInstance(f, _io.BytesIO)
        self.assertEqual(f.getvalue(), b"hello")

    def test_dunder_init_with_none_returns_empty_bytesio_instance(self):
        f = _io.BytesIO(None)
        self.assertEqual(f.getvalue(), b"")

    def test_dunder_init_with_bytes_sets_closed_false(self):
        f = _io.BytesIO(b"hello")
        self.assertFalse(f.closed)
        self.assertTrue(f.readable())
        self.assertTrue(f.writable())
        self.assertTrue(f.seekable())

    def test_dunder_init_with_float_raises_type_error(self):
        with self.assertRaises(TypeError):
            f = _io.BytesIO(0.5)
            f.close()

    def test_close_with_non_BytesIO_self_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.BytesIO.close(5)

    def test_close_clears_buffer(self):
        f = _io.BytesIO(b"foo")
        f.close()
        with self.assertRaises(ValueError):
            f.getvalue()

    def test_close_with_closed_instance_does_not_raise_value_error(self):
        f = _io.BytesIO(b"foo")
        f.close()
        f.close()

    def test_close_with_subclass_overwrite_uses_subclass_property(self):
        class C(_io.BytesIO):
            @property
            def closed(self):
                return 2

        f = C(b"")
        self.assertEqual(f.closed, 2)

    def test_getbuffer_with_non_BytesIO_self_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.BytesIO.getbuffer()

    def test_getbuffer_with_closed_raises_value_error(self):
        f = _io.BytesIO(b"foo")
        f.close()
        with self.assertRaises(ValueError):
            f.getbuffer()

    def test_getvalue_with_non_BytesIO_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.BytesIO.getvalue(5)

    def test_getvalue_with_bytes_returns_bytes_of_buffer(self):
        f = _io.BytesIO(b"foo")
        result = f.getvalue()
        self.assertIsInstance(result, bytes)
        self.assertEqual(result, b"foo")

    def test_getvalue_with_bytearray_returns_bytes_of_buffer(self):
        barr = bytearray(b"foo")
        f = _io.BytesIO(barr)
        result = f.getvalue()
        self.assertIsInstance(result, bytes)
        self.assertEqual(result, b"foo")

    def test_getvalue_with_memoryview_of_bytes_returns_bytes_of_buffer(self):
        view = memoryview(b"foo")
        f = _io.BytesIO(view)
        result = f.getvalue()
        self.assertIsInstance(result, bytes)
        self.assertEqual(result, b"foo")

    def test_getvalue_with_memoryview_of_pointer_returns_bytes_of_buffer(self):
        mm = mmap.mmap(-1, 4)
        view = memoryview(mm)
        f = _io.BytesIO(view)
        result = f.getvalue()
        self.assertIsInstance(result, bytes)
        self.assertEqual(result, b"\x00\x00\x00\x00")

    def test_getvalue_with_modification_on_returned_bytes_returns_same_result(self):
        f = _io.BytesIO(b"foo")
        result = f.getvalue()
        self.assertEqual(result, b"foo")
        result += b"o"
        self.assertEqual(result, b"fooo")
        self.assertEqual(f.getvalue(), b"foo")

    def test_getvalue_with_open_returns_copy_of_value(self):
        f = _io.BytesIO(b"foobarbaz")
        first_value = f.getvalue()
        f.write(b"temp")
        second_value = f.getvalue()
        self.assertEqual(first_value, b"foobarbaz")
        self.assertEqual(second_value, b"temparbaz")

    def test_getvalue_with_closed_raises_value_error(self):
        f = _io.BytesIO(b"hello")
        f.close()
        with self.assertRaises(ValueError):
            f.getvalue()

    def test_getbuffer_with_bytes_returns_bytes_of_buffer(self):
        f = _io.BytesIO(b"foo")
        result = f.getbuffer()
        self.assertIsInstance(result, memoryview)

    def test_read_with_non_BytesIO_self_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.BytesIO.read(5)

    def test_read_one_byte_reads_bytes(self):
        f = _io.BytesIO(b"foo")
        self.assertEqual(f.read(1), b"f")
        self.assertEqual(f.read(1), b"o")
        self.assertEqual(f.read(1), b"o")
        self.assertEqual(f.read(1), b"")
        f.close()

    def test_read_with_bytes_reads_bytes(self):
        f = _io.BytesIO(b"foo")
        self.assertEqual(f.read(), b"foo")
        self.assertEqual(f.read(), b"")
        f.close()

    def test_read_with_none_size_returns_rest(self):
        f = _io.BytesIO(b"foo")
        self.assertEqual(f.read(1), b"f")
        self.assertEqual(f.read(None), b"oo")

    def test_read_with_closed_raises_value_error(self):
        f = _io.BytesIO(b"hello")
        f.close()
        with self.assertRaises(ValueError):
            f.read()

    def test_read_with_int_subclass_reads(self):
        class C(int):
            pass

        f = _io.BytesIO(b"hello")
        c = C(3)
        self.assertEqual(f.read(c), b"hel")

    def test_read_with_int_subclass_does_not_call_dunder_index(self):
        class C(int):
            def __index__(self):
                raise UserWarning("foo")

        f = _io.BytesIO(b"hello")
        c = C()
        f.read(c)

    def test_read_with_non_int_raises_type_error(self):
        f = _io.BytesIO(b"hello")
        with self.assertRaises(TypeError):
            f.read(2.5)

    def test_read1_with_non_BytesIO_self_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.BytesIO.read1(5)

    def test_read1_wtih_bytes_reads_bytes(self):
        f = _io.BytesIO(b"foo")
        self.assertEqual(f.read1(3), b"foo")
        self.assertEqual(f.read1(3), b"")
        f.close()

    def test_read1_with_none_size_returns_rest(self):
        f = _io.BytesIO(b"foo")
        self.assertEqual(f.read1(1), b"f")
        self.assertEqual(f.read1(None), b"oo")

    def test_read1_with_size_not_specified_returns_rest(self):
        f = _io.BytesIO(b"foo")
        self.assertEqual(f.read1(1), b"f")
        self.assertEqual(f.read1(), b"oo")
        f.close()

    # TODO(T47880928): Test BytesIO.readinto
    # TODO(T47880928): Test BytesIO.readinto1

    def test_readline_reads_one_line(self):
        f = _io.BytesIO(b"hello\nworld\nfoo\nbar")
        self.assertEqual(f.readline(), b"hello\n")
        self.assertEqual(f.readline(), b"world\n")
        self.assertEqual(f.readline(), b"foo\n")
        self.assertEqual(f.readline(), b"bar")
        self.assertEqual(f.readline(), b"")
        f.close()

    def test_readlines_reads_all_lines(self):
        f = _io.BytesIO(b"hello\nworld\nfoo\nbar")
        self.assertEqual(f.readlines(), [b"hello\n", b"world\n", b"foo\n", b"bar"])
        self.assertEqual(f.read(), b"")
        f.close()

    def test_seek_with_non_BytesIO_self_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.BytesIO.seek(5, 5)

    def test_seek_with_closed_raises_value_error(self):
        f = _io.BytesIO(b"hello")
        f.close()
        with self.assertRaises(ValueError):
            f.seek(3)

    def test_seek_with_none_offset_raises_type_error(self):
        f = _io.BytesIO(b"hello")
        with self.assertRaises(TypeError):
            f.seek(None, 0)

    def test_seek_with_none_whence_raises_type_error(self):
        f = _io.BytesIO(b"hello")
        with self.assertRaises(TypeError):
            f.seek(1, None)

    def test_seek_with_int_subclass_offset_does_not_call_dunder_index(self):
        class C(int):
            def __index__(self):
                raise UserWarning("foo")

        f = _io.BytesIO(b"")
        pos = C()
        f.seek(pos)

    def test_seek_with_class_offset_calls_dunder_index(self):
        class C:
            def __index__(self):
                raise UserWarning("foo")

        f = _io.BytesIO(b"")
        pos = C()
        with self.assertRaises(UserWarning):
            f.seek(pos)

    def test_seek_with_class_whence_calls_dunder_index(self):
        class C:
            def __index__(self):
                raise UserWarning("foo")

        f = _io.BytesIO(b"")
        whence = C()
        with self.assertRaises(UserWarning):
            f.seek(0, whence)

    def test_seek_with_float_offset_raises_type_error(self):
        f = _io.BytesIO(b"")
        pos = 3.4
        with self.assertRaises(TypeError):
            f.seek(pos)

    def test_seek_with_float_whence_raises_type_error(self):
        f = _io.BytesIO(b"hello")
        with self.assertRaises(TypeError):
            f.seek(0.5)

    def test_seek_with_int_subclass_whence_does_not_call_dunder_index(self):
        class C(int):
            def __index__(self):
                raise UserWarning("foo")

        f = _io.BytesIO(b"")
        whence = C()
        f.seek(5, whence)

    def test_seek_with_whence_0_returns_new_pos(self):
        f = _io.BytesIO(b"")
        pos = 3
        self.assertEqual(f.seek(pos), 3)
        self.assertEqual(f.tell(), 3)

    def test_seek_with_whence_1_returns_new_pos(self):
        f = _io.BytesIO(b"")
        f.seek(3)
        offset = 3
        self.assertEqual(f.seek(offset, 1), 6)
        self.assertEqual(f.tell(), 6)

    def test_seek_with_whence_1_negative_new_pos_retuns_zero(self):
        f = _io.BytesIO(b"")
        f.seek(3)
        offset = -5
        self.assertEqual(f.seek(offset, 1), 0)
        self.assertEqual(f.tell(), 0)

    def test_seek_with_whence_2_returns_new_pos(self):
        f = _io.BytesIO(b"hello")
        offset = 3
        self.assertEqual(f.seek(offset, 2), 8)
        self.assertEqual(f.tell(), 8)

    def test_seek_with_whence_2_negative_new_pos_retuns_zero(self):
        f = _io.BytesIO(b"hello")
        offset = -7
        self.assertEqual(f.seek(offset, 2), 0)
        self.assertEqual(f.tell(), 0)

    def test_seek_with_whence_3_raises_value_error(self):
        f = _io.BytesIO(b"hello")
        with self.assertRaises(ValueError):
            f.seek(3, 3)

    def test_seek_with_large_whence_raises_overflow_error(self):
        f = _io.BytesIO(b"hello")
        with self.assertRaises(OverflowError):
            f.seek(1, 2 ** 65)

    def test_tell_with_non_BytesIO_self_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.BytesIO.tell(5)

    def test_tell_with_closed_file_raises_value_error(self):
        f = _io.BytesIO(b"")
        f.close()
        with self.assertRaises(ValueError):
            f.tell()

    def test_tell_returns_position(self):
        f = _io.BytesIO(b"foo")
        self.assertEqual(f.tell(), 0)
        f.read(1)
        self.assertEqual(f.tell(), 1)

    def test_truncate_with_non_BytesIO_self_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.BytesIO.truncate(5, 5)

    def test_truncate_with_pos_larger_than_length_returns_pos(self):
        f = _io.BytesIO(b"hello")
        self.assertEqual(f.truncate(6), 6)
        self.assertEqual(f.getvalue(), b"hello")

    def test_truncate_with_pos_smaller_than_length_truncates(self):
        f = _io.BytesIO(b"hello")
        self.assertEqual(f.truncate(3), 3)
        self.assertEqual(f.getvalue(), b"hel")

    def test_truncate_with_int_subclass_does_not_call_dunder_int(self):
        class C(int):
            def __int__(self):
                raise UserWarning("foo")

        f = _io.BytesIO(b"")
        pos = C()
        f.truncate(pos)

    def test_truncate_with_non_int_raises_type_error(self):
        dunder_int_called = False

        class C:
            def __int__(self):
                nonlocal dunder_int_called
                dunder_int_called = True
                raise UserWarning("foo")

        f = _io.BytesIO(b"")
        pos = C()
        with self.assertRaises(TypeError):
            f.truncate(pos)
        self.assertFalse(dunder_int_called)

    def test_truncate_with_raising_dunder_int_raises_type_error(self):
        class C:
            def __int__(self):
                raise UserWarning("foo")

        f = _io.BytesIO(b"")
        pos = C()
        with self.assertRaises(TypeError):
            f.truncate(pos)

    def test_truncate_with_float_raises_type_error(self):
        f = _io.BytesIO(b"")
        pos = 3.4
        with self.assertRaises(TypeError):
            f.truncate(pos)

    def test_write_with_non_BytesIO_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.BytesIO.write(5, b"foo")

    def test_write_with_array_returns_length(self):
        bytes_io = _io.BytesIO(b"")
        arr = array.array("b", [1, 2, 3])
        self.assertEqual(bytes_io.write(arr), 3)
        self.assertEqual(bytes_io.getvalue(), b"\x01\x02\x03")

    def test_write_with_bytearray_returns_length(self):
        bytes_io = _io.BytesIO(b"hello")
        bytes_array = bytearray(b"bArr")
        self.assertEqual(bytes_io.write(bytes_array), 4)
        self.assertEqual(bytes_io.getvalue(), b"bArro")

    def test_write_with_bytearray_subclass_returns_length(self):
        class C(bytearray):
            pass

        bytes_io = _io.BytesIO(b"hello")
        instance = C(b"bArr")
        self.assertEqual(bytes_io.write(instance), 4)
        self.assertEqual(bytes_io.getvalue(), b"bArro")

    def test_write_with_bytes_returns_length(self):
        bytes_io = _io.BytesIO(b"hello")
        self.assertEqual(bytes_io.write(b"bytes"), 5)
        self.assertEqual(bytes_io.getvalue(), b"bytes")

    def test_write_with_bytes_subclass_returns_length(self):
        class C(bytes):
            pass

        bytes_io = _io.BytesIO(b"hello")
        instance = C(b"bytes")
        self.assertEqual(bytes_io.write(instance), 5)
        self.assertEqual(bytes_io.getvalue(), b"bytes")

    def test_write_with_memoryview_of_bytearray_returns_length(self):
        bytes_io = _io.BytesIO(b"hello")
        bytes_array = bytearray(b"bArr")
        view = memoryview(bytes_array)
        self.assertEqual(bytes_io.write(view), 4)
        self.assertEqual(bytes_io.getvalue(), b"bArro")

    def test_write_with_memoryview_of_bytes_returns_length(self):
        bytes_io = _io.BytesIO(b"hello")
        view = memoryview(b"view")
        self.assertEqual(bytes_io.write(view), 4)
        self.assertEqual(bytes_io.getvalue(), b"viewo")

    def test_write_with_memoryview_of_pointer_returns_length(self):
        bytes_io = _io.BytesIO(b"")
        mm = mmap.mmap(-1, 4)
        view = memoryview(mm)
        self.assertEqual(bytes_io.write(view), 4)
        self.assertEqual(bytes_io.getvalue(), b"\x00\x00\x00\x00")

    def test_write_with_non_bytes_like_raises_type_error(self):
        bytes_io = _io.BytesIO(b"hello")
        with self.assertRaises(TypeError) as context:
            bytes_io.write(123)
        self.assertRegex(
            str(context.exception), "a bytes-like object is required, not 'int'"
        )
        with self.assertRaises(TypeError) as context:
            bytes_io.write("str")
        self.assertRegex(
            str(context.exception), "a bytes-like object is required, not 'str'"
        )

    def test_write_with_seek_writes_from_current_position(self):
        bytes_io = _io.BytesIO(b"hello")
        self.assertEqual(bytes_io.seek(1), 1)
        self.assertEqual(bytes_io.write(b"1"), 1)
        self.assertEqual(bytes_io.getvalue(), b"h1llo")

    def test_write_with_seek_past_end_pads_with_zero(self):
        bytes_io = _io.BytesIO(b"hello")
        self.assertEqual(bytes_io.seek(7), 7)
        self.assertEqual(bytes_io.write(b"world"), 5)
        self.assertEqual(bytes_io.getvalue(), b"hello\x00\x00world")

    def test_write_with_closed_raises_value_error(self):
        bytes_io = _io.BytesIO(b"hello")
        bytes_io.close()
        with self.assertRaises(ValueError) as context:
            bytes_io.write("close")
        self.assertRegex(str(context.exception), "I/O operation on closed file")

    def test_write_with_empty_does_nothing(self):
        bytes_io = _io.BytesIO(b"hello")
        self.assertEqual(bytes_io.write(b""), 0)
        self.assertEqual(bytes_io.getvalue(), b"hello")

    def test_write_with_larger_bytes_extends_buffer(self):
        bytes_io = _io.BytesIO(b"hello")
        self.assertEqual(bytes_io.write(b"new world"), 9)
        self.assertEqual(bytes_io.getvalue(), b"new world")


class FileIOTests(unittest.TestCase):
    def test_close_closes_file(self):
        f = _io.FileIO(_getfd(), closefd=True)
        self.assertFalse(f.closed)
        f.close()
        self.assertTrue(f.closed)

    def test_close_twice_does_not_raise(self):
        f = _io.FileIO(_getfd(), closefd=True)
        f.close()
        f.close()

    def test_closefd_with_closefd_set_returns_true(self):
        with _io.FileIO(_getfd(), closefd=True) as f:
            self.assertTrue(f.closefd)

    def test_closefd_with_closefd_not_set_returns_true(self):
        with _io.FileIO(_getfd(), closefd=False) as f:
            self.assertFalse(f.closefd)

    def test_dunder_init_with_float_file_raises_type_error(self):
        with self.assertRaises(TypeError) as context:
            _io.FileIO(3.14)

        self.assertEqual(str(context.exception), "integer argument expected, got float")

    def test_dunder_init_with_negative_file_descriptor_raises_value_error(self):
        with self.assertRaises(ValueError) as context:
            _io.FileIO(-5)

        self.assertEqual(str(context.exception), "negative file descriptor")

    def test_dunder_init_with_non_str_mode_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.FileIO(_getfd(), mode=3.14)

    def test_dunder_init_with_no_mode_makes_object_readable(self):
        with _io.FileIO(_getfd()) as f:
            self.assertTrue(f.readable())
            self.assertFalse(f.writable())
            self.assertFalse(f.seekable())

    def test_dunder_init_with_r_in_mode_makes_object_readable(self):
        with _io.FileIO(_getfd(), mode="r") as f:
            self.assertTrue(f.readable())
            self.assertFalse(f.writable())

    def test_dunder_init_with_w_in_mode_makes_object_writable(self):
        with _io.FileIO(_getfd(), mode="w") as f:
            self.assertFalse(f.readable())
            self.assertTrue(f.writable())

    def test_dunder_init_with_r_plus_in_mode_makes_object_readable_and_writable(self):
        with _io.FileIO(_getfd(), mode="r+") as f:
            self.assertTrue(f.readable())
            self.assertTrue(f.writable())

    def test_dunder_init_with_w_plus_in_mode_makes_object_readable_and_writable(self):
        with _io.FileIO(_getfd(), mode="w+") as f:
            self.assertTrue(f.readable())
            self.assertTrue(f.writable())

    def test_dunder_init_with_only_plus_in_mode_raises_value_error(self):
        with self.assertRaises(ValueError) as context:
            _io.FileIO(_getfd(), mode="+")

        self.assertEqual(
            str(context.exception),
            "Must have exactly one of create/read/write/append mode and at "
            "most one plus",
        )

    def test_dunder_init_with_more_than_one_plus_in_mode_raises_value_error(self):
        with self.assertRaises(ValueError) as context:
            _io.FileIO(_getfd(), mode="r++")

        self.assertEqual(
            str(context.exception),
            "Must have exactly one of create/read/write/append mode and at "
            "most one plus",
        )

    def test_dunder_init_with_rw_in_mode_raises_value_error(self):
        with self.assertRaises(ValueError) as context:
            _io.FileIO(_getfd(), mode="rw")

        self.assertEqual(
            str(context.exception),
            "Must have exactly one of create/read/write/append mode and at "
            "most one plus",
        )

    def test_dunder_init_with_filename_and_closefd_equals_false_raises_value_error(
        self,
    ):
        with self.assertRaises(ValueError) as context:
            _io.FileIO("foobar", closefd=False)

        self.assertEqual(
            str(context.exception), "Cannot use closefd=False with file name"
        )

    def test_dunder_init_with_filename_and_opener_calls_opener(self):
        def opener(fn, flags):
            raise UserWarning("foo")

        with self.assertRaises(UserWarning):
            _io.FileIO("foobar", opener=opener)

    def test_dunder_init_with_opener_returning_non_int_raises_type_error(self):
        def opener(fn, flags):
            return "not_an_int"

        with self.assertRaises(TypeError) as context:
            _io.FileIO("foobar", opener=opener)

        self.assertEqual(str(context.exception), "expected integer from opener")

    def test_dunder_init_with_opener_returning_negative_int_raises_value_error(self):
        def opener(fn, flags):
            return -1

        with self.assertRaises(ValueError) as context:
            _io.FileIO("foobar", opener=opener)

        self.assertEqual(str(context.exception), "opener returned -1")

    # TODO(emacs): Enable side-by-side against CPython when issue38031 is fixed
    @pyro_only
    def test_dunder_init_with_opener_returning_bad_fd_raises_os_error(self):
        with self.assertRaises(OSError):
            _io.FileIO("foobar", opener=lambda name, flags: 1000000)

    def test_dunder_init_with_directory_raises_is_a_directory_error(self):
        with self.assertRaises(IsADirectoryError) as context:
            _io.FileIO("/")

        self.assertEqual(context.exception.args, (21, "Is a directory"))

    def test_dunder_repr_with_closed_file_returns_closed_in_result(self):
        f = _io.FileIO(_getfd(), mode="r")
        f.close()
        self.assertEqual(f.__repr__(), "<_io.FileIO [closed]>")

    def test_dunder_repr_with_closefd_and_fd_returns_fd_in_result(self):
        fd = _getfd()
        with _io.FileIO(fd, mode="r", closefd=True) as f:
            self.assertEqual(
                f.__repr__(), f"<_io.FileIO name={fd} mode='rb' closefd=True>"
            )

    def test_dunder_repr_without_closefd_and_fd_returns_fd_in_result(self):
        fd = _getfd()
        with _io.FileIO(fd, mode="r", closefd=False) as f:
            self.assertEqual(
                f.__repr__(), f"<_io.FileIO name={fd} mode='rb' closefd=False>"
            )

    def test_dunder_repr_with_closefd_and_name_returns_name_in_result(self):
        def opener(fn, flags):
            return os.open(fn, os.O_CREAT | os.O_WRONLY, 0o666)

        with tempfile.TemporaryDirectory() as tempdir:
            with _io.FileIO(f"{tempdir}/foo", mode="r", opener=opener) as f:
                self.assertEqual(
                    f.__repr__(),
                    f"<_io.FileIO name='{tempdir}/foo' mode='rb' closefd=True>",
                )

    def test_fileno_with_closed_file_raises_value_error(self):
        f = _io.FileIO(_getfd(), mode="r")
        f.close()
        self.assertRaises(ValueError, f.fileno)

    def test_fileno_returns_int(self):
        with _io.FileIO(_getfd(), mode="r") as f:
            self.assertIsInstance(f.fileno(), int)

    def test_isatty_with_closed_file_raises_value_error(self):
        f = _io.FileIO(_getfd(), mode="r")
        f.close()
        self.assertRaises(ValueError, f.isatty)

    def test_isatty_with_non_tty_returns_false(self):
        with _io.FileIO(_getfd(), mode="r") as f:
            self.assertFalse(f.isatty())

    def test_mode_with_created_and_readable_returns_xb_plus(self):
        with _io.FileIO(_getfd(), mode="x+") as f:
            self.assertEqual(f.mode, "xb+")

    def test_mode_with_created_and_non_readable_returns_xb(self):
        with _io.FileIO(_getfd(), mode="x") as f:
            self.assertEqual(f.mode, "xb")

    # Testing appendable/readable is really tricky.

    def test_mode_with_readable_and_writable_returns_rb_plus(self):
        with _io.FileIO(_getfd(), mode="r+") as f:
            self.assertEqual(f.mode, "rb+")

    def test_mode_with_readable_and_non_writable_returns_rb(self):
        with _io.FileIO(_getfd(), mode="r") as f:
            self.assertEqual(f.mode, "rb")

    def test_mode_with_writable_returns_wb(self):
        with _io.FileIO(_getfd(), mode="w") as f:
            self.assertEqual(f.mode, "wb")

    def test_read_on_closed_file_raises_value_error(self):
        f = _io.FileIO(_getfd(), mode="r")
        f.close()
        self.assertRaises(ValueError, f.read)

    def test_read_on_non_readable_file_raises_unsupported_operation(self):
        with _io.FileIO(_getfd(), mode="w") as f:
            self.assertRaises(_io.UnsupportedOperation, f.read)

    def test_read_returns_bytes(self):
        with _io.FileIO(_getfd(), mode="r") as f:
            self.assertIsInstance(f.read(), bytes)

    def test_readable_with_closed_file_raises_value_error(self):
        f = _io.FileIO(_getfd(), mode="r")
        f.close()
        self.assertRaises(ValueError, f.readable)

    def test_readable_with_readable_file_returns_true(self):
        with _io.FileIO(_getfd(), mode="r") as f:
            self.assertTrue(f.readable())

    def test_readable_with_non_readable_file_returns_false(self):
        with _io.FileIO(_getfd(), mode="w") as f:
            self.assertFalse(f.readable())

    def test_readall_with_non_FileIO_self_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.FileIO.readall(5)

    def test_readall_on_closed_file_raises_value_error(self):
        f = _io.FileIO(_getfd(), mode="r")
        f.close()
        self.assertRaises(ValueError, f.readall)

    # TODO(T70611902): Modify the readall and test after CPython fixed this behavior
    def test_readall_on_non_readable_file_returns_full_bytes(self):
        r, w = os.pipe()
        w = os.fdopen(w, "w")
        w.write("hello")
        w.close()
        with _io.FileIO(r, mode="w") as f:
            self.assertFalse(f.readable())
            self.assertEqual(f.readall(), b"hello")

    def test_readall_returns_bytes(self):
        with _io.FileIO(_getfd(), mode="r") as f:
            self.assertIsInstance(f.readall(), bytes)

    def test_readall_returns_all_bytes(self):
        r, w = os.pipe()
        w = os.fdopen(w, "w")
        w.write("hello")
        w.close()
        with _io.FileIO(r, mode="r") as f:
            result = f.readall()
        self.assertIsInstance(result, bytes)
        self.assertEqual(result, b"hello")

    @unittest.skipIf(sys.platform != "linux", "needs /proc filesystem")
    def test_readall_on_unseekable_file_reads_all(self):
        with open("/proc/self/statm", "rb", buffering=False) as f:
            result = f.readall()
        self.assertGreater(len(result), 1)

    def test_readall_with_more_than_default_buffer_size_returns_all_bytes(self):
        r, w = os.pipe()
        w = os.fdopen(w, "w")
        w.write("foo" * 1000)
        w.close()
        with _io.FileIO(r, mode="r") as f:
            result = f.readall()
        self.assertIsInstance(result, bytes)
        self.assertEqual(result, b"foo" * 1000)

    def test_readall_with_fifo_file_returns_all_bytes(self):
        with tempfile.TemporaryDirectory() as tempdir:
            fifo = f"{tempdir}/fifo"
            os.mkfifo(fifo)
            exe = sys.executable
            src = f"""
import time
with open('{fifo}', 'wb') as f:
    f.write(b'hello...')
    f.flush()
    time.sleep(0.5)
    f.write(b'bye')
"""
            p = subprocess.Popen([exe, "-c", src])
            with open(fifo, "rb", buffering=False) as f:
                result = f.readall()
            self.assertEqual(result, b"hello...bye")
            p.wait()

    # TODO(T53182806): Test FileIO.readinto once memoryview.__setitem__ is done
    def test_readinto_with_non_FileIO_self_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.FileIO.readinto(5, bytearray(b"hello"))

    def test_readinto_with_array_reads_bytes_into_array(self):
        r, w = os.pipe()
        w = os.fdopen(w, "w")
        w.write("hello")
        w.close()
        arr = array.array("b", [1, 2, 3])
        with _io.FileIO(r, mode="r") as f:
            self.assertEqual(f.readinto(arr), 3)
        self.assertEqual(bytes(arr), b"hel")

    def test_readinto_with_bytearray_reads_bytes_into_bytearray(self):
        r, w = os.pipe()
        w = os.fdopen(w, "w")
        w.write("hello")
        w.close()
        barr = bytearray(b"abc")
        with _io.FileIO(r, mode="r") as f:
            self.assertEqual(f.readinto(barr), 3)
        self.assertEqual(barr, b"hel")

    def test_readinto_with_bytes_raises_type_error(self):
        r, w = os.pipe()
        w = os.fdopen(w, "w")
        w.write("hello")
        w.close()
        b = b"abc"
        with _io.FileIO(r, mode="r") as f:
            with self.assertRaises(TypeError):
                f.readinto(b)

    def test_readinto_with_memoryview_of_bytes_raises_type_error(self):
        r, w = os.pipe()
        w = os.fdopen(w, "w")
        w.write("hello")
        w.close()
        view = memoryview(b"abc")
        with _io.FileIO(r, mode="r") as f:
            with self.assertRaises(TypeError):
                f.readinto(view)

    def test_readinto_with_memoryview_of_mmap_reads_bytes_to_pointer(self):
        r, w = os.pipe()
        w = os.fdopen(w, "w")
        w.write("hello")
        w.close()
        mm = mmap.mmap(-1, 2)
        view = memoryview(mm)
        with _io.FileIO(r, mode="r") as f:
            self.assertEqual(f.readinto(view), 2)
        self.assertEqual(bytes(view), b"he")

    def test_readinto_with_mmap_reads_bytes_to_memory(self):
        r, w = os.pipe()
        w = os.fdopen(w, "w")
        w.write("hello")
        w.close()
        mm = mmap.mmap(-1, 2)
        with _io.FileIO(r, mode="r") as f:
            self.assertEqual(f.readinto(mm), 2)
        view = memoryview(mm)
        self.assertEqual(bytes(view), b"he")

    def test_readinto_with_closed_file_raises_value_error(self):
        r, w = os.pipe()
        w = os.fdopen(w, "w")
        w.write("hello")
        w.close()
        barr = bytearray(b"abc")
        with _io.FileIO(r, mode="r") as f:
            f.close()
            with self.assertRaises(ValueError):
                f.readinto(barr)

    def test_readinto_with_empty_bytearray_returns_zero(self):
        r, w = os.pipe()
        w = os.fdopen(w, "w")
        w.write("hello")
        w.close()
        barr = bytearray(b"")
        with _io.FileIO(r, mode="r") as f:
            self.assertEqual(f.readinto(barr), 0)
        self.assertEqual(barr, b"")

    def test_seek_with_float_raises_type_error(self):
        with _io.FileIO(_getfd(), mode="r") as f:
            self.assertRaises(TypeError, f.seek, 3.4)

    def test_seek_with_closed_file_raises_value_error(self):
        f = _io.FileIO(_getfd(), mode="r")
        f.close()
        self.assertRaises(ValueError, f.seek, 3)

    def test_seek_with_non_readable_file_raises_os_error(self):
        with _io.FileIO(_getfd(), mode="w") as f:
            self.assertRaises(OSError, f.seek, 3)

    def test_seekable_with_closed_file_raises_value_error(self):
        f = _io.FileIO(_getfd(), mode="r")
        f.close()
        self.assertRaises(ValueError, f.seekable)

    def test_seekable_does_not_call_subclass_tell(self):
        class C(_io.FileIO):
            def tell(self):
                raise UserWarning("foo")

        with C(_getfd(), mode="w") as f:
            self.assertFalse(f.seekable())

    def test_seekable_with_non_seekable_file_returns_false(self):
        with _io.FileIO(_getfd(), mode="w") as f:
            self.assertFalse(f.seekable())

    def test_supports_arbitrary_attributes(self):
        with _io.FileIO(_getfd(), mode="r") as f:
            f.good_morning = 333
            self.assertEqual(f.good_morning, 333)

    def test_tell_with_closed_file_raises_value_error(self):
        f = _io.FileIO(_getfd(), mode="r")
        f.close()
        self.assertRaises(ValueError, f.tell)

    def test_tell_with_non_readable_file_raises_os_error(self):
        with _io.FileIO(_getfd(), mode="w") as f:
            self.assertRaises(OSError, f.tell)

    def test_truncate_with_closed_file_raises_value_error(self):
        f = _io.FileIO(_getfd(), mode="w")
        f.close()
        self.assertRaises(ValueError, f.truncate)

    def test_truncate_with_non_writable_file_raises_unsupported_operation(self):
        with _io.FileIO(_getfd(), mode="r") as f:
            self.assertRaises(_io.UnsupportedOperation, f.truncate)

    def test_writable_with_closed_file_raises_value_error(self):
        f = _io.FileIO(_getfd(), mode="w")
        f.close()
        self.assertRaises(ValueError, f.writable)

    def test_writable_with_writable_file_returns_true(self):
        with _io.FileIO(_getfd(), mode="w") as f:
            self.assertTrue(f.writable())

    def test_writable_with_non_writable_file_returns_false(self):
        with _io.FileIO(_getfd(), mode="r") as f:
            self.assertFalse(f.writable())

    def test_write_with_non_buffer_raises_type_error(self):
        class C:
            pass

        c = C()
        f = _io.FileIO(_getfd(), mode="w")
        with self.assertRaises(TypeError) as context:
            f.write(c)
        self.assertEqual(
            str(context.exception), "a bytes-like object is required, not 'C'"
        )
        f.close()

    def test_write_with_raising_dunder_buffer_raises_type_error(self):
        class C:
            def __buffer__(self):
                raise UserWarning("foo")

        f = _io.FileIO(_getfd(), mode="w")
        c = C()
        with self.assertRaises(TypeError) as context:
            f.write(c)
        self.assertEqual(
            str(context.exception), "a bytes-like object is required, not 'C'"
        )
        f.close()

    def test_write_with_bad_dunder_buffer_raises_type_error(self):
        class C:
            def __buffer__(self):
                return "foo"

        f = _io.FileIO(_getfd(), mode="w")
        c = C()
        with self.assertRaises(TypeError) as context:
            f.write(c)
        self.assertEqual(
            str(context.exception), "a bytes-like object is required, not 'C'"
        )
        f.close()

    def test_write_writes_bytes(self):
        r, w = os.pipe()
        with _io.FileIO(w, mode="w") as f:
            f.write(b"hello world")
        os.close(r)

    def test_write_writes_bytearray(self):
        r, w = os.pipe()
        with _io.FileIO(w, mode="w") as f:
            f.write(bytearray(b"hello world"))
        os.close(r)


@pyro_only
class FspathTests(unittest.TestCase):
    def test_fspath_with_str_returns_str(self):
        result = "hello"
        self.assertIs(_io._fspath(result), result)

    def test_fspath_with_str_subclass_returns_str(self):
        class C(str):
            pass

        result = C("hello")
        self.assertIs(_io._fspath(result), result)

    def test_fspath_with_str_subclass_does_not_call_dunder_fspath(self):
        class C(str):
            def __fspath__(self):
                raise UserWarning("foo")

        result = C("hello")
        self.assertIs(_io._fspath(result), result)

    def test_fspath_with_bytes_returns_bytes(self):
        result = b"hello"
        self.assertIs(_io._fspath(result), result)

    def test_fspath_with_bytes_subclass_returns_bytes(self):
        class C(bytes):
            pass

        result = C(b"hello")
        self.assertIs(_io._fspath(result), result)

    def test_fspath_with_bytes_subclass_does_not_call_dunder_fspath(self):
        class C(bytes):
            def __fspath__(self):
                raise UserWarning("foo")

        result = C(b"hello")
        self.assertIs(_io._fspath(result), result)

    def test_fspath_with_no_dunder_fspath_raises_type_error(self):
        class C:
            pass

        result = C()
        self.assertRaises(TypeError, _io._fspath, result)

    def test_fspath_calls_dunder_fspath_returns_str(self):
        class C:
            def __fspath__(self):
                return "foo"

        self.assertEqual(_io._fspath(C()), "foo")

    def test_fspath_calls_dunder_fspath_returns_bytes(self):
        class C:
            def __fspath__(self):
                return b"foo"

        self.assertEqual(_io._fspath(C()), b"foo")

    def test_fspath_with_dunder_fspath_returning_non_str_raises_type_error(self):
        class C:
            def __fspath__(self):
                return 5

        result = C()
        self.assertRaises(TypeError, _io._fspath, result)


class OpenTests(unittest.TestCase):
    def test_open_with_bad_mode_raises_type_error(self):
        with self.assertRaisesRegex(
            TypeError, r"open\(\) argument .* must be str, not int"
        ):
            _io.open(0, mode=5)

    def test_open_with_bad_buffering_raises_type_error(self):
        with self.assertRaises(TypeError) as context:
            _io.open(0, buffering=None)

        self.assertEqual(
            str(context.exception), "an integer is required (got type NoneType)"
        )

    def test_open_with_bad_encoding_raises_type_error(self):
        with self.assertRaisesRegex(
            TypeError, r"open\(\) argument .* must be str or None, not int"
        ):
            _io.open(0, encoding=5)

    def test_open_with_bad_errors_raises_type_error(self):
        with self.assertRaisesRegex(
            TypeError, r"open\(\) argument .* must be str or None, not int"
        ):
            _io.open(0, errors=5)

    def test_open_with_duplicate_mode_raises_value_error(self):
        with self.assertRaises(ValueError) as context:
            _io.open(0, mode="rr")

        self.assertEqual(str(context.exception), "invalid mode: 'rr'")

    def test_open_with_bad_mode_character_raises_value_error(self):
        with self.assertRaises(ValueError) as context:
            _io.open(0, mode="q")

        self.assertEqual(str(context.exception), "invalid mode: 'q'")

    def test_open_with_U_and_writing_raises_value_error(self):
        with self.assertRaises(ValueError) as context:
            _io.open(0, mode="Uw")

        self.assertRegex(str(context.exception), "mode U cannot be combined with")

    def test_open_with_U_and_appending_raises_value_error(self):
        with self.assertRaises(ValueError) as context:
            _io.open(0, mode="Ua")

        self.assertRegex(str(context.exception), "mode U cannot be combined with")

    def test_open_with_U_and_updating_raises_value_error(self):
        with self.assertRaises(ValueError) as context:
            _io.open(0, mode="U+")

        self.assertRegex(str(context.exception), "mode U cannot be combined with")

    def test_open_with_text_and_binary_raises_value_error(self):
        with self.assertRaises(ValueError) as context:
            _io.open(0, mode="tb")

        self.assertEqual(
            str(context.exception), "can't have text and binary mode at once"
        )

    def test_open_with_creating_and_reading_raises_value_error(self):
        with self.assertRaises(ValueError) as context:
            _io.open(0, mode="rx")

        self.assertEqual(
            str(context.exception),
            "must have exactly one of create/read/write/append mode",
        )

    def test_open_with_creating_and_writing_raises_value_error(self):
        with self.assertRaises(ValueError) as context:
            _io.open(0, mode="wx")

        self.assertEqual(
            str(context.exception),
            "must have exactly one of create/read/write/append mode",
        )

    def test_open_with_creating_and_appending_raises_value_error(self):
        with self.assertRaises(ValueError) as context:
            _io.open(0, mode="ax")

        self.assertEqual(
            str(context.exception),
            "must have exactly one of create/read/write/append mode",
        )

    def test_open_with_reading_and_writing_raises_value_error(self):
        with self.assertRaises(ValueError) as context:
            _io.open(0, mode="rw")

        self.assertEqual(
            str(context.exception),
            "must have exactly one of create/read/write/append mode",
        )

    def test_open_with_reading_and_appending_raises_value_error(self):
        with self.assertRaises(ValueError) as context:
            _io.open(0, mode="ra")

        self.assertEqual(
            str(context.exception),
            "must have exactly one of create/read/write/append mode",
        )

    def test_open_with_writing_and_appending_raises_value_error(self):
        with self.assertRaises(ValueError) as context:
            _io.open(0, mode="wa")

        self.assertEqual(
            str(context.exception),
            "must have exactly one of create/read/write/append mode",
        )

    def test_open_with_no_reading_creating_writing_appending_raises_value_error(self):
        with self.assertRaises(ValueError) as context:
            _io.open(0, mode="")

        self.assertEqual(
            str(context.exception),
            "Must have exactly one of create/read/write/append mode and at "
            "most one plus",
        )

    def test_open_with_binary_and_encoding_raises_value_error(self):
        with self.assertRaises(ValueError) as context:
            _io.open(0, mode="rb", encoding="utf-8")

        self.assertEqual(
            str(context.exception), "binary mode doesn't take an encoding argument"
        )

    def test_open_with_binary_and_errors_raises_value_error(self):
        with self.assertRaises(ValueError) as context:
            _io.open(0, mode="rb", errors="error")

        self.assertEqual(
            str(context.exception), "binary mode doesn't take an errors argument"
        )

    def test_open_with_binary_and_newline_raises_value_error(self):
        with self.assertRaises(ValueError) as context:
            _io.open(0, mode="rb", newline="nl")

        self.assertEqual(
            str(context.exception), "binary mode doesn't take a newline argument"
        )

    def test_open_with_no_buffering_and_binary_returns_file_io(self):
        fd = _getfd()
        with _io.open(fd, buffering=False, mode="rb") as result:
            self.assertIsInstance(result, _io.FileIO)

    @unittest.skipIf(sys.platform == "darwin", "Darwin rejects non-utf8 filenames")
    def test_open_non_utf8_filename(self):
        with tempfile.TemporaryDirectory() as tempdir:
            filename = "bad_surrogate\udc80.txt"
            full_path = os.path.join(tempdir, filename)
            with open(full_path, "w") as fp:
                fp.write("foobar")
            filename_encoded = filename.encode(
                sys.getfilesystemencoding(), sys.getfilesystemencodeerrors()
            )
            tempdir_bytes = tempdir.encode(
                sys.getfilesystemencoding(), sys.getfilesystemencodeerrors()
            )
            files = os.listdir(tempdir_bytes)
            self.assertIn(filename_encoded, files)

            with open(full_path, "r") as fp:
                self.assertEqual(fp.read(), "foobar")

    def test_open_code_returns_buffered_reader(self):
        with tempfile.TemporaryDirectory() as tempdir:
            filename = "temp.txt"
            full_path = os.path.join(tempdir, filename)

            with open(full_path, "w") as fp:
                fp.write("foobar")

            with _io.open_code(full_path) as result:
                self.assertIsInstance(result, _io.BufferedReader)


@pyro_only
class UnderBufferedIOMixinTests(unittest.TestCase):
    def test_dunder_init_sets_raw(self):
        with _io.FileIO(_getfd(), closefd=True) as f:
            result = _BufferedIOMixin(f)
            self.assertIs(result.raw, f)

    def test_dunder_repr_returns_str(self):
        result = _BufferedIOMixin(None)
        self.assertEqual(result.__repr__(), "<_io._BufferedIOMixin>")

    def test_dunder_repr_gets_raw_name(self):
        class C:
            name = "foo"

        result = _BufferedIOMixin(C())
        self.assertEqual(result.__repr__(), "<_io._BufferedIOMixin name='foo'>")

    def test_close_with_none_raw_raises_value_error(self):
        result = _BufferedIOMixin(None)
        self.assertRaisesRegex(ValueError, "raw stream has been detached", result.close)

    def test_close_with_closed_raw_does_nothing(self):
        class C:
            closed = True

            def close(self):
                pass

        result = _BufferedIOMixin(C())
        self.assertIsNone(result.close())

    def test_close_calls_raw_flush(self):
        # TODO(T53510135): Use unittest.mock
        class C:
            closed = False

            def flush(self):
                raise UserWarning("foo")

            def close(self):
                pass

        result = _BufferedIOMixin(C())
        self.assertRaises(UserWarning, result.close)

    def test_close_calls_raw_close(self):
        # TODO(T53510135): Use unittest.mock
        class C:
            closed = False

            def close(self):
                raise UserWarning("foo")

        result = _BufferedIOMixin(C())
        self.assertRaises(UserWarning, result.close)

    def test_closed_with_none_raw_raises_value_error(self):
        result = _BufferedIOMixin(None)
        with self.assertRaisesRegex(ValueError, "raw stream has been detached"):
            result.closed

    def test_closed_calls_raw_closed(self):
        # TODO(T53510135): Use unittest.mock
        class C:
            @property
            def closed(self):
                raise UserWarning("foo")

        result = _BufferedIOMixin(C())
        with self.assertRaises(UserWarning):
            result.closed

    def test_detach_with_none_raw_raises_value_error(self):
        result = _BufferedIOMixin(None)
        self.assertRaisesRegex(
            ValueError, "raw stream has been detached", result.detach
        )

    def test_detach_calls_raw_flush(self):
        # TODO(T53510135): Use unittest.mock
        class C:
            closed = False

            def flush(self):
                raise UserWarning("foo")

        result = _BufferedIOMixin(C())
        self.assertRaises(UserWarning, result.detach)

    def test_detach_returns_raw_and_sets_none(self):
        with _io.BytesIO() as raw:
            result = _BufferedIOMixin(raw)
            self.assertIs(result.detach(), raw)
            self.assertIs(result.raw, None)

    def test_fileno_with_none_raw_raises_value_error(self):
        result = _BufferedIOMixin(None)
        self.assertRaisesRegex(
            ValueError, "raw stream has been detached", result.fileno
        )

    def test_fileno_calls_raw_fileno(self):
        # TODO(T53510135): Use unittest.mock
        class C:
            closed = False

            def fileno(self):
                raise UserWarning("foo")

        result = _BufferedIOMixin(C())
        self.assertRaises(UserWarning, result.fileno)

    def test_flush_with_none_raw_raises_value_error(self):
        result = _BufferedIOMixin(None)
        self.assertRaisesRegex(ValueError, "raw stream has been detached", result.flush)

    def test_flush_with_closed_raw_raises_value_error(self):
        class C:
            closed = True

            def close(self):
                pass

        result = _BufferedIOMixin(C())
        self.assertRaises(ValueError, result.flush)

    def test_flush_calls_raw_flush(self):
        # TODO(T53510135): Use unittest.mock
        class C:
            closed = False

            def flush(self):
                raise UserWarning("foo")

        result = _BufferedIOMixin(C())
        self.assertRaises(UserWarning, result.flush)

    def test_isatty_calls_raw_isatty(self):
        # TODO(T53510135): Use unittest.mock
        class C:
            closed = False

            def isatty(self):
                raise UserWarning("foo")

        result = _BufferedIOMixin(C())
        self.assertRaises(UserWarning, result.isatty)

    def test_isatty_with_none_raw_raises_value_error(self):
        result = _BufferedIOMixin(None)
        self.assertRaisesRegex(
            ValueError, "raw stream has been detached", result.isatty
        )

    def test_mode_calls_raw_mode(self):
        # TODO(T53510135): Use unittest.mock
        class C:
            @property
            def mode(self):
                raise UserWarning("foo")

        result = _BufferedIOMixin(C())
        with self.assertRaises(UserWarning):
            result.mode

    def test_name_calls_raw_name(self):
        # TODO(T53510135): Use unittest.mock
        class C:
            @property
            def name(self):
                raise UserWarning("foo")

        result = _BufferedIOMixin(C())
        with self.assertRaises(UserWarning):
            result.name

    def test_seek_with_none_raw_raises_value_error(self):
        result = _BufferedIOMixin(None)
        with self.assertRaisesRegex(ValueError, "raw stream has been detached"):
            result.seek(0)

    def test_tell_with_none_raw_raises_value_error(self):
        result = _BufferedIOMixin(None)
        self.assertRaisesRegex(ValueError, "raw stream has been detached", result.tell)

    def test_tell_calls_raw_tell(self):
        # TODO(T53510135): Use unittest.mock
        class C:
            def tell(self):
                raise UserWarning("foo")

        result = _BufferedIOMixin(C())
        self.assertRaises(UserWarning, result.tell)

    def test_tell_with_raw_tell_returning_negative_raises_os_error(self):
        class C:
            def tell(self):
                return -1

        result = _BufferedIOMixin(C())
        self.assertRaises(OSError, result.tell)

    def test_tell_returns_result_of_raw_tell(self):
        class C:
            def tell(self):
                return 5

        result = _BufferedIOMixin(C())
        self.assertEqual(result.tell(), 5)

    def test_truncate_calls_raw_flush(self):
        # TODO(T53510135): Use unittest.mock
        class C:
            closed = False

            def flush(self):
                raise UserWarning("foo")

        result = _BufferedIOMixin(C())
        self.assertRaises(UserWarning, result.truncate)

    def test_truncate_with_none_pos_calls_tell(self):
        # TODO(T53510135): Use unittest.mock
        class C:
            closed = False

            def flush(self):
                pass

            def tell(self):
                raise UserWarning("foo")

        result = _BufferedIOMixin(C())
        self.assertRaises(UserWarning, result.truncate)

    def test_truncate_with_none_pos_calls_raw_truncate_with_pos(self):
        # TODO(T53510135): Use unittest.mock
        class C:
            closed = False

            def flush(self):
                pass

            def tell(self):
                return 5

            def truncate(self, pos):
                raise UserWarning(pos)

        result = _BufferedIOMixin(C())
        with self.assertRaises(UserWarning) as context:
            result.truncate()
        self.assertEqual(context.exception.args, (5,))

    def test_truncate_calls_raw_truncate(self):
        # TODO(T53510135): Use unittest.mock
        class C:
            closed = False

            def flush(self):
                pass

            def tell(self):
                raise MemoryError("foo")

            def truncate(self, pos):
                raise UserWarning(pos)

        result = _BufferedIOMixin(C())
        with self.assertRaises(UserWarning) as context:
            result.truncate(10)
        self.assertEqual(context.exception.args, (10,))

    def test_seek_calls_raw_seek(self):
        # TODO(T53510135): Use unittest.mock
        class C:
            def seek(self, pos, whence):
                raise UserWarning(pos, whence)

        result = _BufferedIOMixin(C())
        self.assertRaises(UserWarning, result.seek, 5, 0)

    def tes_seek_returning_negative_pos_raises_os_error(self):
        class C:
            def seek(self, pos, whence):
                return -1

        result = _BufferedIOMixin(C())
        self.assertRaises(OSError, result.seek, 5, 0)

    def test_seek_returns_result_of_raw_seek(self):
        class C:
            def seek(self, pos, whence):
                return 100

        result = _BufferedIOMixin(C())
        self.assertEqual(result.seek(5, 0), 100)

    def test_seekable_calls_raw_seekable(self):
        # TODO(T53510135): Use unittest.mock
        class C:
            closed = False

            def seekable(self):
                raise UserWarning("foo")

        result = _BufferedIOMixin(C())
        self.assertRaises(UserWarning, result.seekable)


class IncrementalNewlineDecoderTests(unittest.TestCase):
    def test_dunder_init_with_non_int_translate_raises_type_error(self):
        with self.assertRaises(TypeError) as context:
            _io.IncrementalNewlineDecoder(None, translate="foo")
        self.assertEqual(
            str(context.exception), "an integer is required (got type str)"
        )

    def test_decode_with_none_decoder_uses_input(self):
        decoder = _io.IncrementalNewlineDecoder(decoder=None, translate=1)
        self.assertEqual(decoder.decode("bar"), "bar")

    def test_decode_with_non_int_final_raises_type_error(self):
        decoder = _io.IncrementalNewlineDecoder(decoder=None, translate=1)
        with self.assertRaises(TypeError) as context:
            decoder.decode("bar", final="foo")
        self.assertEqual(
            str(context.exception), "an integer is required (got type str)"
        )

    def test_decode_with_decoder_calls_decoder_on_input(self):
        class C:
            def decode(self, input, final):
                raise UserWarning(input, final)

        decoder = _io.IncrementalNewlineDecoder(decoder=C(), translate=1)
        with self.assertRaises(UserWarning) as context:
            decoder.decode("foo", 5)
        self.assertEqual(context.exception.args, ("foo", True))

    def test_decode_with_no_newlines_returns_input(self):
        decoder = _io.IncrementalNewlineDecoder(decoder=None, translate=1)
        self.assertEqual(decoder.decode("bar"), "bar")
        self.assertEqual(decoder.newlines, None)
        self.assertEqual(decoder.getstate(), (b"", 0))

    def test_decode_with_carriage_return_does_not_translate_to_newline(self):
        decoder = _io.IncrementalNewlineDecoder(decoder=None, translate=0)
        self.assertEqual(decoder.decode("bar\r\n"), "bar\r\n")
        self.assertEqual(decoder.newlines, "\r\n")
        self.assertEqual(decoder.getstate(), (b"", 0))

    def test_decode_with_trailing_carriage_removes_carriage(self):
        decoder = _io.IncrementalNewlineDecoder(decoder=None, translate=0)
        self.assertEqual(decoder.decode("bar\r"), "bar")
        self.assertEqual(decoder.newlines, None)
        self.assertEqual(decoder.getstate(), (b"", 1))

    def test_decode_with_carriage_does_not_translate_to_newline(self):
        decoder = _io.IncrementalNewlineDecoder(decoder=None, translate=0)
        self.assertEqual(decoder.decode("bar\rbaz"), "bar\rbaz")
        self.assertEqual(decoder.newlines, "\r")
        self.assertEqual(decoder.getstate(), (b"", 0))

    def test_decode_translate_with_carriage_return_translates_to_newline(self):
        decoder = _io.IncrementalNewlineDecoder(decoder=None, translate=1)
        self.assertEqual(decoder.decode("bar\r\n"), "bar\n")
        self.assertEqual(decoder.newlines, "\r\n")
        self.assertEqual(decoder.getstate(), (b"", 0))

    def test_decode_translate_with_carriage_translates_to_newline(self):
        decoder = _io.IncrementalNewlineDecoder(decoder=None, translate=1)
        self.assertEqual(decoder.decode("bar\rbaz"), "bar\nbaz")
        self.assertEqual(decoder.newlines, "\r")
        self.assertEqual(decoder.getstate(), (b"", 0))

    def test_getstate_with_decoder_calls_decoder_getstate(self):
        class C:
            def decode(self, input, final):
                return input

            def getstate(self):
                return (b"foo", 5)

        decoder = _io.IncrementalNewlineDecoder(decoder=C(), translate=1)
        self.assertEqual(decoder.getstate(), (b"foo", 10))

    def test_getstate_with_decoder_calls_decoder_getstate_carriage(self):
        class C:
            def decode(self, input, final):
                return input

            def getstate(self):
                return (b"foo", 5)

        decoder = _io.IncrementalNewlineDecoder(decoder=C(), translate=1)
        decoder.decode("bar\r")
        self.assertEqual(decoder.getstate(), (b"foo", 11))

    def test_setstate_with_decoder_calls_decoder_setstate(self):
        set_state = None

        class C:
            def decode(self, input, final):
                return input

            def getstate(self):
                return b"bar", 1

            def setstate(self, state):
                nonlocal set_state
                set_state = state

        decoder = _io.IncrementalNewlineDecoder(decoder=C(), translate=1)
        self.assertIsNone(decoder.setstate((b"foo", 4)))
        decoder.decode("bar\r")
        self.assertEqual(set_state, (b"foo", 2))
        self.assertEqual(decoder.getstate(), (b"bar", 3))

    def test_setstate_with_decoder_calls_decoder_setstate_carriage(self):
        set_state = None

        class C:
            def decode(self, input, final):
                return input

            def getstate(self):
                return b"bar", 1

            def setstate(self, state):
                nonlocal set_state
                set_state = state

        decoder = _io.IncrementalNewlineDecoder(decoder=C(), translate=1)
        self.assertIsNone(decoder.setstate((b"foo", 4)))
        decoder.decode("bar\r")
        self.assertEqual(set_state, (b"foo", 2))
        self.assertEqual(decoder.getstate(), (b"bar", 3))

    def test_reset_changes_newlines(self):
        decoder = _io.IncrementalNewlineDecoder(decoder=None, translate=1)
        decoder.decode("bar\rbaz")
        self.assertEqual(decoder.newlines, "\r")
        self.assertIsNone(decoder.reset())
        self.assertEqual(decoder.newlines, None)

    def test_reset_changes_pendingcr(self):
        decoder = _io.IncrementalNewlineDecoder(decoder=None, translate=1)
        decoder.decode("bar\r")
        self.assertEqual(decoder.getstate(), (b"", 1))
        self.assertIsNone(decoder.reset())
        self.assertEqual(decoder.getstate(), (b"", 0))

    def test_reset_calls_decoder_reset(self):
        class C:
            def reset(self):
                raise UserWarning("foo")

        decoder = _io.IncrementalNewlineDecoder(decoder=C(), translate=1)
        self.assertRaises(UserWarning, decoder.reset)


class UnderTextIOBaseTests(unittest.TestCase):
    def test_read_in_dict(self):
        self.assertIn("read", _io._TextIOBase.__dict__)

    def test_write_in_dict(self):
        self.assertIn("write", _io._TextIOBase.__dict__)

    def test_readline_in_dict(self):
        self.assertIn("readline", _io._TextIOBase.__dict__)

    def test_detach_in_dict(self):
        self.assertIn("detach", _io._TextIOBase.__dict__)

    def test_encoding_in_dict(self):
        self.assertIn("encoding", _io._TextIOBase.__dict__)

    def test_newlines_in_dict(self):
        self.assertIn("newlines", _io._TextIOBase.__dict__)

    def test_errors_in_dict(self):
        self.assertIn("errors", _io._TextIOBase.__dict__)


class BufferedReaderTests(unittest.TestCase):
    def test_dunder_init_with_non_readable_stream_raises_os_error(self):
        with _io.FileIO(_getfd(), mode="w") as file_reader:
            with self.assertRaises(OSError) as context:
                _io.BufferedReader(file_reader)
        self.assertEqual(str(context.exception), "File or stream is not readable.")

    def test_dunder_init_allows_subclasses(self):
        class C(_io.BufferedReader):
            pass

        instance = C(_io.BytesIO(b""))
        self.assertIsInstance(instance, _io.BufferedReader)

    def test_peek_returns_buffered_data(self):
        with _io.BufferedReader(_io.BytesIO(b"hello"), buffer_size=3) as buffered_io:
            self.assertEqual(buffered_io.peek(-10), b"hel")
            self.assertEqual(buffered_io.peek(1), b"hel")
            self.assertEqual(buffered_io.peek(2), b"hel")

    def test_raw_returns_raw(self):
        with _io.BytesIO(b"hello") as bytes_io:
            with _io.BufferedReader(bytes_io) as buffered_io:
                self.assertIs(buffered_io.raw, bytes_io)

    def test_read_with_detached_stream_raises_value_error(self):
        with _io.FileIO(_getfd(), mode="r") as file_reader:
            buffered = _io.BufferedReader(file_reader)
            buffered.detach()
            with self.assertRaises(ValueError) as context:
                buffered.read()
        self.assertEqual(str(context.exception), "raw stream has been detached")

    def test_read_with_closed_stream_raises_value_error(self):
        file_reader = _io.FileIO(_getfd(), mode="r")
        buffered = _io.BufferedReader(file_reader)
        file_reader.close()
        with self.assertRaises(ValueError) as context:
            buffered.read()
        self.assertIn("closed file", str(context.exception))

    def test_read_with_negative_size_raises_value_error(self):
        with _io.FileIO(_getfd(), mode="r") as file_reader:
            buffered = _io.BufferedReader(file_reader)
            with self.assertRaises(ValueError) as context:
                buffered.read(-2)
        self.assertEqual(
            str(context.exception), "read length must be non-negative or -1"
        )

    def test_read_with_none_size_calls_raw_readall(self):
        class C(_io.FileIO):
            def readall(self):
                raise UserWarning("foo")

        with C(_getfd(), mode="r") as file_reader:
            buffered = _io.BufferedReader(file_reader)
            with self.assertRaises(UserWarning) as context:
                buffered.read(None)
        self.assertEqual(context.exception.args, ("foo",))

    def test_read_reads_bytes(self):
        with _io.BytesIO(b"hello") as bytes_io:
            buffered = _io.BufferedReader(bytes_io, buffer_size=1)
            result = buffered.read()
            self.assertEqual(result, b"hello")

    def test_read_reads_count_bytes(self):
        with _io.BytesIO(b"hello") as bytes_io:
            buffered = _io.BufferedReader(bytes_io, buffer_size=1)
            result = buffered.read(3)
            self.assertEqual(result, b"hel")

    def test_readinto_writes_to_buffer(self):
        with _io.BytesIO(b"hello") as bytes_io:
            with _io.BufferedReader(bytes_io, buffer_size=4) as buffered:
                ba = bytearray(b"XXXXXXXXXXXX")
                buffered.readinto(ba)
                self.assertEqual(ba, bytearray(b"helloXXXXXXX"))

    def test_read1_calls_read(self):
        with _io.BytesIO(b"hello") as bytes_io:
            buffered = _io.BufferedReader(bytes_io, buffer_size=10)
            result = buffered.read1(3)
            self.assertEqual(result, b"hel")

    def test_read1_reads_from_buffer(self):
        with _io.BytesIO(b"hello") as bytes_io:
            buffered = _io.BufferedReader(bytes_io, buffer_size=4)
            buffered.read(1)
            result = buffered.read1(10)
            self.assertEqual(result, b"ell")

    def test_read1_with_size_not_specified_returns_rest_in_buffer(self):
        with _io.BytesIO(b"hello") as bytes_io:
            buffered = _io.BufferedReader(bytes_io, buffer_size=4)
            buffered.read(1)
            result = buffered.read1()
            self.assertEqual(result, b"ell")

    def test_read1_with_none_size_raises_type_error(self):
        with _io.BytesIO(b"hello") as bytes_io:
            buffered = _io.BufferedReader(bytes_io, buffer_size=4)
            with self.assertRaises(TypeError):
                buffered.read1(None)

    def test_readable_calls_raw_readable(self):
        readable_calls = 0

        class C:
            closed = False

            def readable(self):
                nonlocal readable_calls
                readable_calls += 1
                return True

        result = _io.BufferedReader(C())
        self.assertEqual(readable_calls, 1)
        self.assertTrue(result.readable())
        self.assertEqual(readable_calls, 2)

    def test_tell_returns_current_position(self):
        with _io.BytesIO(b"hello") as bytes_io:
            buffered = _io.BufferedReader(bytes_io)
            self.assertEqual(buffered.tell(), 0)
            buffered.read(2)
            self.assertEqual(buffered.tell(), 2)

    def test_seek_with_invalid_whence(self):
        with _io.BytesIO(b"hello") as bytes_io:
            buffered = _io.BufferedReader(bytes_io)
            self.assertRaises(ValueError, buffered.seek, 0, 4)
            self.assertRaises(ValueError, buffered.seek, 0, -1)

    def test_seek_resets_position(self):
        with _io.BytesIO(b"hello") as bytes_io:
            buffered = _io.BufferedReader(bytes_io)
            buffered.read(2)
            self.assertEqual(buffered.tell(), 2)
            buffered.seek(0)
            self.assertEqual(buffered.tell(), 0)

    def test_supports_arbitrary_attributes(self):
        with _io.BytesIO(b"hello") as bytes_io:
            buffered = _io.BufferedReader(bytes_io)
            buffered.buenos_dias = 1234
            self.assertEqual(buffered.buenos_dias, 1234)


class TextIOWrapperTests(unittest.TestCase):
    def _sample(self):
        return _io.TextIOWrapper(_io.BytesIO(b"hello++"), encoding="ascii")

    def _sample_utf8(self):
        return _io.TextIOWrapper(_io.BytesIO(b"hello++"), encoding="UTF-8")

    def _sample_utf16(self):
        return _io.TextIOWrapper(_io.BytesIO(b"hello++"), encoding="UTF-16")

    def _sample_latin1(self):
        return _io.TextIOWrapper(_io.BytesIO(b"hello++"), encoding="Latin-1")

    def _sample_buffered_writer(self):
        return _io.BufferedWriter(_io.BytesIO(b"hello++"), buffer_size=3)

    def _sample_buffered_reader(self):
        return _io.BufferedReader(_io.BytesIO(b"hello++"), buffer_size=3)

    def test_dunder_init_with_none_buffer_raises_attribute_error(self):
        with self.assertRaises(AttributeError) as context:
            _io.TextIOWrapper(None)
        self.assertEqual(
            str(context.exception), "'NoneType' object has no attribute 'readable'"
        )

    def test_dunder_init_with_non_str_encoding_raises_type_error(self):
        with self.assertRaisesRegex(
            TypeError, r"TextIOWrapper\(\) argument .* must be str or None, not int"
        ):
            _io.TextIOWrapper("hello", encoding=5)

    def test_dunder_init_with_non_str_errors_raises_type_error(self):
        with self.assertRaisesRegex(
            TypeError, r"TextIOWrapper\(\) argument .* must be str or None, not int"
        ):
            _io.TextIOWrapper("hello", errors=5)

    def test_dunder_init_with_non_str_newline_raises_type_error(self):
        with self.assertRaisesRegex(
            TypeError, r"TextIOWrapper\(\) argument .* must be str or None, not int"
        ):
            _io.TextIOWrapper("hello", newline=5)

    def test_dunder_init_with_non_int_line_buffering_raises_type_error(self):
        with self.assertRaises(TypeError) as context:
            _io.TextIOWrapper("hello", line_buffering="buf")
        self.assertEqual(
            str(context.exception), "an integer is required (got type str)"
        )

    def test_dunder_init_with_non_int_write_through_raises_type_error(self):
        with self.assertRaises(TypeError) as context:
            _io.TextIOWrapper("hello", write_through="True")
        self.assertEqual(
            str(context.exception), "an integer is required (got type str)"
        )

    def test_dunder_init_with_bad_str_newline_raises_value_error(self):
        with self.assertRaises(ValueError) as context:
            _io.TextIOWrapper("hello", newline="foo")
        self.assertEqual(str(context.exception), "illegal newline value: foo")

    def test_dunder_init_returns_text_io_wrapper(self):
        with self._sample() as text_io:
            self.assertIsInstance(text_io, _io.TextIOWrapper)

    def test_dunder_repr(self):
        with self._sample() as text_io:
            self.assertEqual(text_io.__repr__(), "<_io.TextIOWrapper encoding='ascii'>")

    def test_dunder_repr_with_name(self):
        class C(_io.TextIOWrapper):
            name = "foo"

        with C(_io.BytesIO(b"hello"), encoding="ascii") as text_io:
            self.assertEqual(
                text_io.__repr__(), "<_io.TextIOWrapper name='foo' encoding='ascii'>"
            )

    def test_dunder_repr_with_mode(self):
        # TODO(T54575279): remove workaround and use subclass again
        with _io.TextIOWrapper(_io.BytesIO(b"hello"), encoding="ascii") as text_io:
            text_io.mode = "rwx"
            self.assertEqual(
                repr(text_io), "<_io.TextIOWrapper mode='rwx' encoding='ascii'>"
            )

    def test_buffer_returns_buffer(self):
        with _io.BytesIO() as bytes_io:
            with _io.TextIOWrapper(bytes_io) as text_io:
                self.assertIs(text_io.buffer, bytes_io)

    def test_close_with_non_TextIOWrapper_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.TextIOWrapper.close(5)

    def test_close_with_detached_buffer_raises_value_error(self):
        text_io = self._sample()
        text_io.detach()
        self.assertRaisesRegex(
            ValueError, "underlying buffer has been detached", text_io.close
        )

    def test_closed_with_detached_buffer_raises_value_error(self):
        text_io = self._sample()
        text_io.detach()
        with self.assertRaisesRegex(ValueError, "underlying buffer has been detached"):
            text_io.closed

    def test_detach_with_non_TextIOWrapper_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.TextIOWrapper.detach(5)

    def test_detach_with_detached_buffer_raises_value_error(self):
        text_io = self._sample()
        text_io.detach()
        self.assertRaisesRegex(
            ValueError, "underlying buffer has been detached", text_io.detach
        )

    def test_encoding_returns_encoding(self):
        with self._sample() as text_io:
            self.assertEqual(text_io.encoding, "ascii")

    def test_default_encoding_is_UTF8(self):
        with _io.BytesIO() as bytes_io:
            with _io.TextIOWrapper(bytes_io) as text_io:
                self.assertEqual(text_io.encoding, "UTF-8")

    def test_errors_returns_default_errors(self):
        with self._sample() as text_io:
            self.assertEqual(text_io.errors, "strict")

    def test_errors_returns_errors(self):
        with _io.TextIOWrapper(_io.BytesIO(b"hello"), errors="foobar") as text_io:
            self.assertEqual(text_io.errors, "foobar")

    def test_line_buffering_returns_line_buffering(self):
        with _io.TextIOWrapper(_io.BytesIO(b"hello"), line_buffering=5) as text_io:
            self.assertEqual(text_io.line_buffering, True)

    def test_name_with_bytes_io_raises_attribute_error(self):
        with self._sample() as text_io:
            with self.assertRaisesRegex(AttributeError, "name"):
                text_io.name

    def test_name_with_detached_buffer_raises_value_error(self):
        text_io = self._sample()
        text_io.detach()
        with self.assertRaisesRegex(ValueError, "underlying buffer has been detached"):
            text_io.name

    def test_name_returns_buffer_name(self):
        class C(_io.BytesIO):
            name = "foobar"

        with _io.TextIOWrapper(C(b"hello")) as text_io:
            self.assertEqual(text_io.name, "foobar")

    def test_fileno_with_non_TextIOWrapper_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.TextIOWrapper.fileno(5)

    def test_fileno_with_bytes_io_raises_unsupported_operation(self):
        with self._sample() as text_io:
            with self.assertRaisesRegex(_io.UnsupportedOperation, "fileno"):
                text_io.fileno()

    def test_fileno_with_detached_buffer_raises_value_error(self):
        text_io = self._sample()
        text_io.detach()
        self.assertRaisesRegex(
            ValueError, "underlying buffer has been detached", text_io.fileno
        )

    def test_fileno_returns_buffer_fileno(self):
        class C(_io.BytesIO):
            def fileno(self):
                return 5

        with _io.TextIOWrapper(C(b"hello")) as text_io:
            self.assertEqual(text_io.fileno(), 5)

    def test_isatty_with_non_TextIOWrapper_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.TextIOWrapper.isatty(5)

    def test_isatty_with_bytes_io_returns_false(self):
        with self._sample() as text_io:
            self.assertFalse(text_io.isatty())

    def test_isatty_with_detached_buffer_raises_value_error(self):
        text_io = self._sample()
        text_io.detach()
        self.assertRaisesRegex(
            ValueError, "underlying buffer has been detached", text_io.isatty
        )

    def test_isatty_returns_buffer_isatty(self):
        class C(_io.BytesIO):
            def isatty(self):
                return True

        with _io.TextIOWrapper(C(b"hello")) as text_io:
            self.assertTrue(text_io.isatty())

    def test_newlines_with_detached_buffer_raises_value_error(self):
        text_io = self._sample()
        text_io.detach()
        with self.assertRaisesRegex(ValueError, "underlying buffer has been detached"):
            text_io.newlines

    def test_newlines_returns_newlines(self):
        with _io.TextIOWrapper(_io.BytesIO(b"\rhello\n")) as text_io:
            text_io.read()
            self.assertEqual(text_io.newlines, ("\r", "\n"))

    def test_seek_with_non_TextIOWrapper_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.TextIOWrapper.seek(5, 5)

    def test_seek_with_detached_buffer_raises_value_error(self):
        text_io = self._sample()
        text_io.detach()
        with self.assertRaisesRegex(ValueError, "underlying buffer has been detached"):
            text_io.seek(0)

    def test_seek_with_detached_buffer_and_non_int_whence_raises_type_error(self):
        text_io = self._sample()
        text_io.detach()
        with self.assertRaises(TypeError):
            text_io.seek(5, 5.5)

    def test_seekable_with_non_TextIOWrapper_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.TextIOWrapper.seekable(5)

    def test_seekable_with_detached_buffer_raises_value_error(self):
        text_io = self._sample()
        text_io.detach()
        self.assertRaisesRegex(
            ValueError, "underlying buffer has been detached", text_io.seekable
        )

    def test_seekable_with_closed_io_raises_value_error(self):
        text_io = self._sample()
        text_io.close()
        self.assertRaises(ValueError, text_io.seekable)

    def test_readable_with_non_TextIOWrapper_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.TextIOWrapper.readable(5)

    def test_readable_with_detached_buffer_raises_value_error(self):
        text_io = self._sample()
        text_io.detach()
        self.assertRaisesRegex(
            ValueError, "underlying buffer has been detached", text_io.readable
        )

    def test_readable_calls_buffer_readable(self):
        class C(_io.BytesIO):
            readable = Mock(name="readable")

        with C(b"hello") as bytes_io:
            with _io.TextIOWrapper(bytes_io) as text_io:
                bytes_io.readable.assert_called_once()
                text_io.readable()
                self.assertEqual(bytes_io.readable.call_count, 2)

    def test_supports_arbitrary_attributes(self):
        text_io = self._sample()
        text_io.heya = ()
        self.assertEqual(text_io.heya, ())

    def test_writable_with_non_TextIOWrapper_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.TextIOWrapper.writable(5)

    def test_writable_with_detached_buffer_raises_value_error(self):
        text_io = self._sample()
        text_io.detach()
        self.assertRaisesRegex(
            ValueError, "underlying buffer has been detached", text_io.writable
        )

    def test_writable_calls_buffer_writable(self):
        class C(_io.BytesIO):
            writable = Mock(name="writable")

        with C(b"hello") as bytes_io:
            with _io.TextIOWrapper(bytes_io) as text_io:
                bytes_io.writable.assert_called_once()
                text_io.writable()
                self.assertEqual(bytes_io.writable.call_count, 2)

    def test_flush_with_non_TextIOWrapper_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.TextIOWrapper.flush(5)

    def test_flush_with_closed_buffer_raises_value_error(self):
        text_io = self._sample()
        text_io.close()
        self.assertRaisesRegex(ValueError, "closed", text_io.flush)

    def test_flush_with_detached_buffer_raises_value_error(self):
        text_io = self._sample()
        text_io.detach()
        self.assertRaisesRegex(
            ValueError, "underlying buffer has been detached", text_io.flush
        )

    def test_flush_calls_buffer_flush(self):
        class C(_io.BytesIO):
            flush = Mock(name="flush", return_value=None)

        with C(b"hello") as bytes_io:
            with _io.TextIOWrapper(bytes_io) as text_io:
                bytes_io.flush.assert_not_called()
                self.assertIsNone(text_io.flush())
                bytes_io.flush.assert_called_once()

    def test_read_with_non_TextIOWrapper_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.TextIOWrapper.read(5)

    def test_read_with_detached_buffer_raises_value_error(self):
        text_io = self._sample()
        text_io.detach()
        self.assertRaisesRegex(
            ValueError, "underlying buffer has been detached", text_io.read
        )

    def test_read_with_detached_buffer_and_non_int_size_raises_type_error(self):
        text_io = self._sample()
        text_io.detach()
        with self.assertRaises(TypeError):
            text_io.read(5.5)

    def test_read_reads_chars(self):
        with _io.TextIOWrapper(_io.BytesIO(b"foo bar")) as text_io:
            result = text_io.read(3)
            self.assertEqual(result, "foo")
            self.assertEqual(text_io.read(), " bar")

    def test_readline_with_non_TextIOWrapper_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.TextIOWrapper.readline(5)

    def test_readline_with_closed_file_raises_value_error(self):
        text_io = self._sample()
        text_io.close()
        self.assertRaisesRegex(ValueError, "closed file", text_io.readline)

    def test_readline_with_detached_buffer_raises_value_error(self):
        text_io = self._sample()
        text_io.detach()
        self.assertRaisesRegex(
            ValueError, "underlying buffer has been detached", text_io.readline
        )

    def test_readline_with_non_int_size_raises_type_error(self):
        with self._sample() as text_io:
            self.assertRaisesRegex(
                TypeError,
                "cannot be interpreted as an integer",
                text_io.readline,
                "not_int",
            )

    def test_readline_reads_line_terminated_by_newline(self):
        with _io.TextIOWrapper(_io.BytesIO(b"foo\nbar")) as text_io:
            self.assertEqual(text_io.readline(), "foo\n")
            self.assertEqual(text_io.readline(), "bar")
            self.assertEqual(text_io.readline(), "")

    def test_readline_reads_line_terminated_by_cr(self):
        with _io.TextIOWrapper(_io.BytesIO(b"foo\rbar")) as text_io:
            self.assertEqual(text_io.readline(), "foo\n")
            self.assertEqual(text_io.readline(), "bar")
            self.assertEqual(text_io.readline(), "")

    def test_readline_reads_line_terminated_by_crlf(self):
        with _io.TextIOWrapper(_io.BytesIO(b"foo\r\nbar")) as text_io:
            self.assertEqual(text_io.readline(), "foo\n")
            self.assertEqual(text_io.readline(), "bar")
            self.assertEqual(text_io.readline(), "")

    def test_readline_reads_line_with_only_cr(self):
        with _io.TextIOWrapper(_io.BytesIO(b"\r")) as text_io:
            self.assertEqual(text_io.readline(), "\n")
            self.assertEqual(text_io.readline(), "")

    def test_tell_with_non_TextIOWrapper_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.TextIOWrapper.tell(5)

    def test_tell_with_detached_buffer_raises_value_error(self):
        text_io = self._sample()
        text_io.detach()
        self.assertRaisesRegex(
            ValueError, "underlying buffer has been detached", text_io.tell
        )

    def test_truncate_with_non_TextIOWrapper_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.TextIOWrapper.truncate(5, 5)

    def test_truncate_with_closed_buffer_raises_value_error(self):
        text_io = self._sample()
        text_io.close()
        self.assertRaisesRegex(ValueError, "closed", text_io.truncate)

    def test_truncate_with_detached_buffer_raises_value_error(self):
        text_io = self._sample()
        text_io.detach()
        self.assertRaisesRegex(
            ValueError, "underlying buffer has been detached", text_io.truncate
        )

    def test_truncate_calls_buffer_flush(self):
        class C(_io.BytesIO):
            flush = Mock(name="flush")

        with C(b"hello") as bytes_io:
            with _io.TextIOWrapper(C()) as text_io:
                bytes_io.flush.assert_not_called()
                text_io.truncate()
                bytes_io.flush.assert_called_once()

    def test_truncate_calls_buffer_truncate(self):
        class C(_io.BytesIO):
            truncate = Mock(name="truncate")

        with C(b"hello") as bytes_io:
            with _io.TextIOWrapper(C()) as text_io:
                bytes_io.truncate.assert_not_called()
                text_io.truncate()
                bytes_io.truncate.assert_called_once()

    def test_write_with_non_TextIOWrapper_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.TextIOWrapper.write(5, "foo")

    def test_write_with_detached_buffer_raises_value_error(self):
        text_io = self._sample()
        text_io.detach()
        with self.assertRaisesRegex(ValueError, "underlying buffer has been detached"):
            text_io.write("foo")

    def test_write_with_detach_buffer_and_non_str_text_raises_type_error(self):
        text_io = self._sample()
        text_io.detach()
        with self.assertRaises(TypeError):
            text_io.write(5.5)

    def test_write_with_ascii_encoding_and_letters_writes_chars(self):
        with self._sample() as text_io:
            self.assertEqual(text_io.write("foo"), 3)
            text_io.seek(0)
            self.assertEqual(text_io.read(), "foolo++")

    def test_write_with_ascii_encoding_and_special_characters_writes_chars(self):
        with self._sample() as text_io:
            self.assertEqual(text_io.write("!?+-()"), 6)
            text_io.seek(0)
            self.assertEqual(text_io.read(), "!?+-()+")

    def test_write_with_utf8_encoding_and_letters_writes_chars(self):
        with self._sample_utf8() as text_io:
            self.assertEqual(text_io.write("foo"), 3)
            text_io.seek(0)
            self.assertEqual(text_io.read(), "foolo++")

    def test_write_with_utf8_encoding_and_special_characters_writes_chars(self):
        with self._sample_utf8() as text_io:
            self.assertEqual(text_io.write("(("), 2)
            text_io.seek(0)
            self.assertEqual(text_io.read(), "((llo++")

    def test_write_with_utf8_encoding_and_strict_errors_and_surrogate_raises_unicodeencode_error(
        self,
    ):
        text_io = _io.TextIOWrapper(
            _io.BytesIO(b"hello"), encoding="UTF-8", errors="strict"
        )
        with self.assertRaises(ValueError):
            text_io.write("ab\uD800++")

    def test_write_with_utf8_encoding_and_ignore_errors_removes_surrogates(self):
        text_io = _io.TextIOWrapper(
            _io.BytesIO(b"hello++"), encoding="UTF-8", errors="ignore"
        )
        self.assertEqual(text_io.write("ab\uD800a"), 4)
        text_io.seek(0)
        self.assertEqual(text_io.read(), "abalo++")

    def test_write_with_utf8_encoding_and_replace_errors_replaces_surrogates_with_question_mark(
        self,
    ):
        text_io = _io.TextIOWrapper(
            _io.BytesIO(b"hello++"), encoding="UTF-8", errors="replace"
        )
        self.assertEqual(text_io.write("ab\uD800"), 3)
        text_io.seek(0)
        self.assertEqual(text_io.read(), "ab?lo++")

    # TODO(T61927696): Test write with "surrogatepass" errors when "surrogatepass" is supported by decode

    def test_write_with_latin1_encoding_and_letters_writes_chars(self):
        with self._sample_latin1() as text_io:
            self.assertEqual(text_io.write("foo"), 3)
            text_io.seek(0)
            self.assertEqual(text_io.read(), "foolo++")

    def test_write_with_latin1_encoding_and_special_characters_writes_chars(self):
        with self._sample_latin1() as text_io:
            self.assertEqual(text_io.write("(("), 2)
            text_io.seek(0)
            self.assertEqual(text_io.read(), "((llo++")

    def test_write_with_bufferedwriter_ascii_encoding_and_letters_returns_length_written(
        self,
    ):
        with _io.TextIOWrapper(
            self._sample_buffered_writer(), encoding="ascii"
        ) as text_io:
            self.assertEqual(text_io.write("foo"), 3)

    def test_write_with_bufferedwriter_ascii_encoding_and_special_characters_returns_length_written(
        self,
    ):
        with _io.TextIOWrapper(
            self._sample_buffered_writer(), encoding="ascii"
        ) as text_io:
            self.assertEqual(text_io.write("!?+-()"), 6)

    def test_write_with_bufferedwriter_utf8_encoding_and_letters_returns_length_written(
        self,
    ):
        with _io.TextIOWrapper(
            self._sample_buffered_writer(), encoding="UTF-8"
        ) as text_io:
            self.assertEqual(text_io.write("foo"), 3)

    def test_write_with_bufferedwriter_utf8_encoding_and_special_characters_returns_length_written(
        self,
    ):
        with _io.TextIOWrapper(
            self._sample_buffered_writer(), encoding="UTF-8"
        ) as text_io:
            self.assertEqual(text_io.write("(("), 2)

    def test_write_with_bufferedwriter_utf8_encoding_and_strict_errors_and_surrogate_raises_unicodeencode_error(
        self,
    ):
        with _io.TextIOWrapper(
            self._sample_buffered_writer(), encoding="UTF-8", errors="strict"
        ) as text_io:
            with self.assertRaises(ValueError):
                text_io.write("ab\uD800++")

    def test_write_with_bufferedwriter_utf8_encoding_and_ignore_errors_returns_length_written(
        self,
    ):
        with _io.TextIOWrapper(
            self._sample_buffered_writer(), encoding="UTF-8", errors="ignore"
        ) as text_io:
            self.assertEqual(text_io.write("ab\uD800a"), 4)

    def test_write_with_bufferedwriter_with_utf8_encoding_and_replace_errors_returns_length_written(
        self,
    ):
        with _io.TextIOWrapper(
            self._sample_buffered_writer(), encoding="UTF-8", errors="replace"
        ) as text_io:
            self.assertEqual(text_io.write("ab\uD800"), 3)

    # TODO(T61927696): Test write with "surrogatepass" errors when "surrogatepass" is supported by decode

    def test_write_with_bufferedwriter_latin1_encoding_and_letters_returns_length_written(
        self,
    ):
        with _io.TextIOWrapper(
            self._sample_buffered_writer(), encoding="Latin-1"
        ) as text_io:
            self.assertEqual(text_io.write("foo"), 3)

    def test_write_with_bufferedwriter_latin1_encoding_and_special_characters_returns_length_written(
        self,
    ):
        with _io.TextIOWrapper(
            self._sample_buffered_writer(), encoding="Latin-1"
        ) as text_io:
            self.assertEqual(text_io.write("(("), 2)


class StringIOTests(unittest.TestCase):
    def test_dunder_init_with_non_str_initial_value_raises_type_error(self):
        self.assertRaisesRegex(
            TypeError,
            "initial_value must be str or None",
            _io.StringIO,
            initial_value=b"foo",
        )

    def test_dunder_init_sets_initial_value(self):
        with _io.StringIO(initial_value="foo") as string_io:
            self.assertEqual(string_io.getvalue(), "foo")

    def test_dunder_init_with_non_str_or_none_newline_raises_value_error(self):
        with self.assertRaises(TypeError) as context:
            _io.StringIO(newline=1)
        self.assertRegex(str(context.exception), "newline must be str or None, not int")

    def test_dunder_init_with_illegal_newline_raises_value_error(self):
        with self.assertRaises(ValueError) as context:
            _io.StringIO(newline="n")
        self.assertRegex(str(context.exception), "illegal newline value")

    def test_dunder_repr(self):
        with _io.StringIO(initial_value="foo") as string_io:
            self.assertRegex(string_io.__repr__(), r"<_io.StringIO object at 0x\w+>")

    def test_close_with_non_StringIO_self_raises_type_error(self):
        with self.assertRaises(TypeError):
            _io.StringIO.close(0.5)

    def test_close_with_StringIO_subclass_sets_closed_true(self):
        class C(_io.StringIO):
            pass

        sio = C("foo")

        self.assertFalse(sio.closed)
        sio.close()
        self.assertTrue(sio.closed)

    def test_detach_raises_unsupported_operation(self):
        with _io.StringIO() as string_io:
            self.assertRaisesRegex(_io.UnsupportedOperation, "detach", string_io.detach)

    def test_encoding_always_returns_none(self):
        with _io.StringIO() as string_io:
            self.assertIsNone(string_io.encoding)

    def test_errors_always_returns_none(self):
        with _io.StringIO() as string_io:
            self.assertIsNone(string_io.errors)

    def test_line_buffering_with_open_returns_false(self):
        with _io.StringIO() as string_io:
            self.assertFalse(string_io.line_buffering)

    def test_line_buffering_with_closed_returns_raises_value_error(self):
        string_io = _io.StringIO()
        string_io.close()
        with self.assertRaises(ValueError) as context:
            string_io.line_buffering
        self.assertRegex(str(context.exception), "I/O operation on closed file")

    def test_getvalue_with_non_stringio_raises_type_error(self):
        self.assertRaisesRegex(
            TypeError,
            r"'getvalue' .* '(_io\.)?StringIO' object.* a 'int'",
            _io.StringIO.getvalue,
            1,
        )

    def test_getvalue_with_open_returns_copy_of_value(self):
        strio = _io.StringIO("foobarbaz")
        first_value = strio.getvalue()
        strio.write("temp")
        second_value = strio.getvalue()
        self.assertEqual(first_value, "foobarbaz")
        self.assertEqual(second_value, "temparbaz")

    def test_getvalue_with_closed_raises_value_error(self):
        string_io = _io.StringIO()
        string_io.close()
        with self.assertRaises(ValueError) as context:
            string_io.getvalue()
        self.assertRegex(str(context.exception), "I/O operation on closed file")

    def test_getvalue_subclass_and_closed_attr_returns_value(self):
        class Closed(_io.StringIO):
            closed = True

        closed = Closed("foobar")
        self.assertEqual(closed.getvalue(), "foobar")

    def test_iter_with_open_returns_self(self):
        string_io = _io.StringIO()
        self.assertEqual(string_io, string_io.__iter__())

    def test_iter_with_closed_raises_value_error(self):
        string_io = _io.StringIO()
        string_io.close()
        with self.assertRaises(ValueError) as context:
            string_io.__iter__()
        self.assertRegex(str(context.exception), "I/O operation on closed file")

    def test_iter_subclass_and_closed_attr_raises_value_error(self):
        class Closed(_io.StringIO):
            closed = True

        closed = Closed()
        with self.assertRaises(ValueError) as context:
            closed.__iter__()
        self.assertRegex(str(context.exception), "I/O operation on closed file")

    def test_next_with_non_stringio_raises_type_error(self):
        self.assertRaisesRegex(
            TypeError,
            r"'__next__' .* '(_io\.)?StringIO' object.* a 'int'",
            _io.StringIO.__next__,
            1,
        )

    def test_next_with_open_reads_line(self):
        string_io = _io.StringIO("foo\nbar")
        self.assertEqual(string_io.__next__(), "foo\n")
        self.assertEqual(string_io.__next__(), "bar")
        with self.assertRaises(StopIteration):
            string_io.__next__()

    def test_next_with_closed_raises_value_error(self):
        string_io = _io.StringIO()
        string_io.close()
        with self.assertRaises(ValueError) as context:
            string_io.__next__()
        self.assertRegex(str(context.exception), "I/O operation on closed file")

    def test_next_subclass_and_closed_attr_reads_line(self):
        class Closed(_io.StringIO):
            closed = True

        closed = Closed("foo\nbar")
        self.assertEqual(closed.__next__(), "foo\n")
        self.assertEqual(closed.__next__(), "bar")
        with self.assertRaises(StopIteration):
            closed.__next__()

    def test_newline_default(self):
        strio = _io.StringIO("a\nb\r\nc\rd")
        self.assertEqual(list(strio), ["a\n", "b\r\n", "c\rd"])
        self.assertEqual(strio.getvalue(), "a\nb\r\nc\rd")

        strio = _io.StringIO()
        self.assertEqual(strio.write("a\nb\r\nc\rd"), 8)
        strio.seek(0)
        self.assertEqual(list(strio), ["a\n", "b\r\n", "c\rd"])
        self.assertEqual(strio.getvalue(), "a\nb\r\nc\rd")

    def test_newline_none(self):
        strio = _io.StringIO("a\nb\r\nc\rd", newline=None)
        self.assertEqual(list(strio), ["a\n", "b\n", "c\n", "d"])
        strio.seek(0)
        self.assertEqual(strio.read(1), "a")
        self.assertEqual(strio.read(2), "\nb")
        self.assertEqual(strio.read(2), "\nc")
        self.assertEqual(strio.read(1), "\n")
        self.assertEqual(strio.getvalue(), "a\nb\nc\nd")

        strio = _io.StringIO(newline=None)
        self.assertEqual(2, strio.write("a\n"))
        self.assertEqual(3, strio.write("b\r\n"))
        self.assertEqual(3, strio.write("c\rd"))
        strio.seek(0)
        self.assertEqual(strio.read(), "a\nb\nc\nd")
        self.assertEqual(strio.getvalue(), "a\nb\nc\nd")

        strio = _io.StringIO("a\r\nb", newline=None)
        self.assertEqual(strio.read(3), "a\nb")

    def test_newline_none_with_r_end_not_raise_exception(self):
        strio = _io.StringIO("a\nb\r\nc\r", newline=None)
        self.assertEqual(list(strio), ["a\n", "b\n", "c\n"])

    def test_newline_empty(self):
        strio = _io.StringIO("a\nb\r\nc\rd", newline="")
        self.assertEqual(list(strio), ["a\n", "b\r\n", "c\r", "d"])
        strio.seek(0)
        self.assertEqual(strio.read(4), "a\nb\r")
        self.assertEqual(strio.read(2), "\nc")
        self.assertEqual(strio.read(1), "\r")
        self.assertEqual(strio.getvalue(), "a\nb\r\nc\rd")

        strio = _io.StringIO(newline="")
        self.assertEqual(2, strio.write("a\n"))
        self.assertEqual(2, strio.write("b\r"))
        self.assertEqual(2, strio.write("\nc"))
        self.assertEqual(2, strio.write("\rd"))
        strio.seek(0)
        self.assertEqual(list(strio), ["a\n", "b\r\n", "c\r", "d"])
        self.assertEqual(strio.getvalue(), "a\nb\r\nc\rd")

    def test_newline_lf(self):
        strio = _io.StringIO("a\nb\r\nc\rd", newline="\n")
        self.assertEqual(list(strio), ["a\n", "b\r\n", "c\rd"])
        self.assertEqual(strio.getvalue(), "a\nb\r\nc\rd")

        strio = _io.StringIO(newline="\n")
        self.assertEqual(strio.write("a\nb\r\nc\rd"), 8)
        strio.seek(0)
        self.assertEqual(list(strio), ["a\n", "b\r\n", "c\rd"])
        self.assertEqual(strio.getvalue(), "a\nb\r\nc\rd")

    def test_newline_cr(self):
        strio = _io.StringIO("a\nb\r\nc\rd", newline="\r")
        self.assertEqual(strio.read(), "a\rb\r\rc\rd")
        strio.seek(0)
        self.assertEqual(list(strio), ["a\r", "b\r", "\r", "c\r", "d"])
        self.assertEqual(strio.getvalue(), "a\rb\r\rc\rd")

        strio = _io.StringIO(newline="\r")
        self.assertEqual(strio.write("a\nb\r\nc\rd"), 8)
        strio.seek(0)
        self.assertEqual(list(strio), ["a\r", "b\r", "\r", "c\r", "d"])
        strio.seek(0)
        self.assertEqual(strio.readlines(), ["a\r", "b\r", "\r", "c\r", "d"])
        self.assertEqual(strio.getvalue(), "a\rb\r\rc\rd")

    def test_newline_crlf(self):
        strio = _io.StringIO("a\nb\r\nc\rd", newline="\r\n")
        self.assertEqual(strio.read(), "a\r\nb\r\r\nc\rd")
        strio.seek(0)
        self.assertEqual(list(strio), ["a\r\n", "b\r\r\n", "c\rd"])
        strio.seek(0)
        self.assertEqual(strio.readlines(), ["a\r\n", "b\r\r\n", "c\rd"])
        self.assertEqual(strio.getvalue(), "a\r\nb\r\r\nc\rd")

        strio = _io.StringIO(newline="\r\n")
        self.assertEqual(strio.write("a\nb\r\nc\rd"), 8)
        strio.seek(0)
        self.assertEqual(list(strio), ["a\r\n", "b\r\r\n", "c\rd"])
        self.assertEqual(strio.getvalue(), "a\r\nb\r\r\nc\rd")

    def test_read_with_open_returns_string(self):
        string_io = _io.StringIO("foo\n")
        self.assertEqual(string_io.read(), "foo\n")

    def test_read_with_closed_raises_value_error(self):
        string_io = _io.StringIO()
        string_io.close()
        with self.assertRaises(ValueError) as context:
            string_io.read()
        self.assertRegex(str(context.exception), "I/O operation on closed file")

    def test_read_with_subclass_and_closed_attr_returns_string(self):
        class Closed(_io.StringIO):
            closed = True

        closed = Closed("foo\n")
        self.assertEqual(closed.read(), "foo\n")

    def test_read_with_non_int_raises_type_error(self):
        string_io = _io.StringIO("hello world")
        with self.assertRaises(TypeError):
            string_io.read("not-int")

    # CPython 3.6 does not do the __index__ call, whereas >= 3.7 does. Since we've
    # already implemented this functionality, we should test it.
    @pyro_only
    def test_read_with_intlike_calls_dunder_index(self):
        class IntLike:
            def __index__(self):
                return 5

        string_io = _io.StringIO("hello world")
        self.assertEqual(string_io.read(IntLike()), "hello")

    def test_read_starts_at_internal_pos(self):
        string_io = _io.StringIO("hello world")
        string_io.seek(6)
        self.assertEqual(string_io.read(), "world")

    def test_read_stops_after_passed_in_number_of_bytes(self):
        string_io = _io.StringIO("hello world")
        self.assertEqual(string_io.read(5), "hello")

    def test_read_with_negative_size_returns_remaining_string(self):
        string_io = _io.StringIO("hello world")
        self.assertEqual(string_io.read(-5), "hello world")

    def test_read_with_overseek_returns_empty_string(self):
        string_io = _io.StringIO("hello world")
        string_io.seek(20)
        self.assertEqual(string_io.read(), "")

    def test_read_translates_all_newlines_if_newline_is_none(self):
        string_io = _io.StringIO("foo\nbar\rbaz\r\n", None)
        self.assertEqual(string_io.read(), "foo\nbar\nbaz\n")

    def test_read_doesnt_translate_if_newline_is_empty(self):
        string_io = _io.StringIO("foo\nbar\rbaz\r\n", "")
        self.assertEqual(string_io.read(), "foo\nbar\rbaz\r\n")

    def test_read_translates_line_feeds_if_newline_is_string(self):
        string_io = _io.StringIO("foo\nbar\rbaz\r\n", "\r\n")
        self.assertEqual(string_io.read(), "foo\r\nbar\rbaz\r\r\n")

    def test_readline_with_open_returns_string(self):
        string_io = _io.StringIO("foo\nbar")
        self.assertEqual(string_io.readline(), "foo\n")

    def test_readline_with_closed_raises_value_error(self):
        string_io = _io.StringIO()
        string_io.close()
        with self.assertRaises(ValueError) as context:
            string_io.readline()
        self.assertRegex(str(context.exception), "I/O operation on closed file")

    def test_readline_with_subclass_and_closed_attr_returns_string(self):
        class Closed(_io.StringIO):
            closed = True

        closed = Closed("foo\nbar")
        self.assertEqual(closed.readline(), "foo\n")

    def test_readline_with_non_int_raises_type_error(self):
        string_io = _io.StringIO("hello world")
        with self.assertRaises(TypeError):
            string_io.readline("not-int")

    # CPython 3.6 does not do the __index__ call, whereas >= 3.7 does. Since we've
    # already implemented this functionality, we should test it.
    @pyro_only
    def test_readline_with_intlike_calls_dunder_index(self):
        class IntLike:
            def __index__(self):
                return 5

        string_io = _io.StringIO("hello world")
        self.assertEqual(string_io.readline(IntLike()), "hello")

    def test_readline_starts_at_internal_pos(self):
        string_io = _io.StringIO("hello world")
        string_io.seek(6)
        self.assertEqual(string_io.readline(), "world")

    def test_readline_stops_after_passed_in_number_of_bytes(self):
        string_io = _io.StringIO("hello world\n")
        self.assertEqual(string_io.readline(5), "hello")

    def test_readline_with_negative_size_full_line(self):
        string_io = _io.StringIO("hello world\n")
        self.assertEqual(string_io.readline(-5), "hello world\n")

    def test_readline_with_overseek_returns_empty_string(self):
        string_io = _io.StringIO("hello world")
        string_io.seek(20)
        self.assertEqual(string_io.readline(), "")

    def test_readline_translates_all_newlines_if_newline_is_none(self):
        string_io = _io.StringIO("foo\nbar\rbaz\r\n", None)
        self.assertEqual(string_io.readline(), "foo\n")
        self.assertEqual(string_io.readline(), "bar\n")
        self.assertEqual(string_io.readline(), "baz\n")

    def test_readline_doesnt_translate_if_newline_is_empty(self):
        string_io = _io.StringIO("foo\nbar\rbaz\r\n", "")
        self.assertEqual(string_io.readline(), "foo\n")
        self.assertEqual(string_io.readline(), "bar\r")
        self.assertEqual(string_io.readline(), "baz\r\n")

    def test_readline_translates_line_feeds_if_newline_is_string(self):
        string_io = _io.StringIO("foo\nbar\rbaz\r\n", "\r\n")
        self.assertEqual(string_io.readline(), "foo\r\n")
        self.assertEqual(string_io.readline(), "bar\rbaz\r\r\n")

    def test_readable_with_open_StringIO_returns_true(self):
        string_io = _io.StringIO()
        self.assertTrue(string_io.readable())

    def test_readable_with_closed_StringIO_raises_value_error(self):
        string_io = _io.StringIO()
        string_io.close()
        with self.assertRaises(ValueError) as context:
            string_io.readable()
        self.assertRegex(str(context.exception), "I/O operation on closed file")

    def test_seek_with_open_sets_position(self):
        string_io = _io.StringIO("foo\n")
        self.assertEqual(string_io.seek(2), 2)
        self.assertEqual(string_io.tell(), 2)

    def test_seek_with_closed_raises_value_error(self):
        string_io = _io.StringIO()
        string_io.close()
        with self.assertRaises(ValueError) as context:
            string_io.seek(0)
        self.assertRegex(str(context.exception), "I/O operation on closed file")

    def test_seek_with_subclass_and_closed_attr_sets_position(self):
        class Closed(_io.StringIO):
            closed = True

        closed = Closed("foo\n")
        self.assertEqual(closed.seek(2), 2)
        self.assertEqual(closed.tell(), 2)

    def test_seek_with_non_int_cookie_raises_type_error(self):
        string_io = _io.StringIO("hello world")
        with self.assertRaises(TypeError):
            string_io.seek("not-int", 0)

    def test_seek_with_intlike_cookie_calls_dunder_index(self):
        class IntLike:
            def __index__(self):
                return 5

        string_io = _io.StringIO("hello world")
        self.assertEqual(string_io.seek(IntLike()), 5)
        self.assertEqual(string_io.tell(), 5)

    def test_seek_with_non_int_whence_raises_type_error(self):
        string_io = _io.StringIO("hello world")
        with self.assertRaises(TypeError):
            string_io.seek(0, "not-int")

    def test_seek_accepts_index_covertible_whence(self):
        class IndexLike:
            def __index__(self):
                return 0

        string_io = _io.StringIO("hello world")
        self.assertEqual(string_io.seek(1, IndexLike()), 1)

    def test_seek_can_overseek(self):
        string_io = _io.StringIO("foo\n")
        self.assertEqual(string_io.seek(10), 10)
        self.assertEqual(string_io.tell(), 10)

    def test_seek_with_whence_zero_and_negative_cookie_raises_value_error(self):
        string_io = _io.StringIO("foo\n")
        with self.assertRaises(ValueError) as context:
            string_io.seek(-10)
        self.assertRegex(str(context.exception), "Negative seek position -10")

    def test_seek_with_whence_one_and_non_zero_cookie_raises_os_error(self):
        string_io = _io.StringIO("foo\n")
        with self.assertRaises(OSError) as context:
            string_io.seek(1, 1)
        self.assertRegex(str(context.exception), "Can't do nonzero cur-relative seeks")

    def test_seek_with_whence_one_doesnt_change_pos(self):
        string_io = _io.StringIO("foo\n")
        self.assertEqual(string_io.seek(2), 2)
        self.assertEqual(string_io.seek(0, 1), 2)
        self.assertEqual(string_io.tell(), 2)

    def test_seek_with_whence_two_and_non_zero_cookie_raises_os_error(self):
        string_io = _io.StringIO("foo\n")
        with self.assertRaises(OSError) as context:
            string_io.seek(1, 1)
        self.assertRegex(str(context.exception), "Can't do nonzero cur-relative seeks")

    def test_seek_with_whence_two_sets_pos_to_end(self):
        string_io = _io.StringIO("foo\n")
        self.assertEqual(string_io.seek(2), 2)
        self.assertEqual(string_io.seek(0, 2), 4)
        self.assertEqual(string_io.tell(), 4)

    def test_seek_with_invalid_whence_raises_value_error(self):
        string_io = _io.StringIO("foo\n")
        with self.assertRaises(ValueError) as context:
            string_io.seek(1, 3)
        self.assertEqual(
            str(context.exception), "Invalid whence (3, should be 0, 1 or 2)"
        )

    def test_seek_with_bigint_whence_raises_overflow_error(self):
        string_io = _io.StringIO("foo\n")
        with self.assertRaisesRegex(
            OverflowError, r"Python int too large to convert to C .*"
        ):
            string_io.seek(1, 2 ** 65)

    def test_seekable_with_open_StringIO_returns_true(self):
        string_io = _io.StringIO()
        self.assertTrue(string_io.seekable())

    def test_seekable_with_closed_StringIO_raises_value_error(self):
        string_io = _io.StringIO()
        string_io.close()
        with self.assertRaises(ValueError) as context:
            string_io.seekable()
        self.assertRegex(str(context.exception), "I/O operation on closed file")

    def test_subclass_with_closed_attribute_is_not_closed_for_StringIO(self):
        class Closed(_io.StringIO):
            closed = True

        c = Closed()
        self.assertEqual(c.write("foobar"), 6)
        self.assertEqual(c.seek(0), 0)
        with self.assertRaises(ValueError) as context:
            # Since readlines is inherited from `_IOBase which checks for the
            # closed property, this method call should raise a ValueError
            c.readlines()
        self.assertRegex(str(context.exception), "I/O operation on closed file")

    def test_supports_arbitrary_attributes(self):
        string_io = _io.StringIO()
        string_io.here_comes_the_sun = "and I say it's alright"
        self.assertEqual(string_io.here_comes_the_sun, "and I say it's alright")

    def test_tell_with_open_returns_position(self):
        string_io = _io.StringIO()
        self.assertEqual(string_io.seek(2), 2)
        self.assertEqual(string_io.tell(), 2)

    def test_tell_with_closed_raises_value_error(self):
        string_io = _io.StringIO()
        string_io.close()
        with self.assertRaises(ValueError) as context:
            string_io.tell()
        self.assertRegex(str(context.exception), "I/O operation on closed file")

    def test_tell_with_subclass_and_closed_attr_returns_string(self):
        class Closed(_io.StringIO):
            closed = True

        closed = Closed("foo\nbar")
        self.assertEqual(closed.tell(), 0)

    def test_truncate_with_open_truncates_from_pos(self):
        string_io = _io.StringIO("hello world")
        string_io.seek(5)
        self.assertEqual(string_io.truncate(), 5)
        self.assertEqual(string_io.getvalue(), "hello")

    def test_truncate_with_closed_raises_value_error(self):
        string_io = _io.StringIO()
        string_io.close()
        with self.assertRaises(ValueError) as context:
            string_io.truncate()
        self.assertRegex(str(context.exception), "I/O operation on closed file")

    def test_truncate_with_subclass_and_closed_attr_returns_string(self):
        class Closed(_io.StringIO):
            closed = True

        closed = Closed("hello world")
        closed.seek(5)
        self.assertEqual(closed.truncate(), 5)
        self.assertEqual(closed.getvalue(), "hello")

    def test_truncate_with_non_int_raises_type_error(self):
        string_io = _io.StringIO("hello world")
        with self.assertRaises(TypeError):
            string_io.truncate("not-int")

    # CPython 3.6 does not do the __index__ call, whereas >= 3.7 does. Since we've
    # already implemented this functionality, we should test it.
    @pyro_only
    def test_truncate_with_intlike_calls_dunder_index(self):
        class IntLike:
            def __index__(self):
                return 5

        string_io = _io.StringIO("hello world")
        self.assertEqual(string_io.truncate(IntLike()), 5)
        self.assertEqual(string_io.getvalue(), "hello")

    def test_truncate_with_negative_int_raises_value_error(self):
        string_io = _io.StringIO("hello world")
        with self.assertRaises(ValueError) as context:
            string_io.truncate(-1)
        self.assertRegex(str(context.exception), "Negative size value -1")

    def test_truncate_with_size_past_end_of_buffer_returns_size(self):
        string_io = _io.StringIO("hello")
        self.assertEqual(string_io.truncate(10), 10)
        self.assertEqual(string_io.getvalue(), "hello")

    def test_write_with_open_writes_to_object(self):
        string_io = _io.StringIO()
        self.assertEqual(string_io.write("foobar"), 6)
        self.assertEqual(string_io.getvalue(), "foobar")

    def test_write_with_closed_raises_value_error(self):
        strio = _io.StringIO()
        self.assertEqual(strio.write("foobar"), 6)
        strio.close()
        with self.assertRaises(ValueError) as context:
            strio.write("foo")
        self.assertRegex(str(context.exception), "I/O operation on closed file")

    def test_write_with_subclass_and_closed_attr_sets_position(self):
        class Closed(_io.StringIO):
            closed = True

        closed = Closed()
        self.assertEqual(closed.write("foobar"), 6)
        self.assertEqual(closed.getvalue(), "foobar")

    def test_write_with_non_string_raises_type_error(self):
        strio = _io.StringIO()
        with self.assertRaises(TypeError):
            strio.write(1)

    def test_write_starts_writing_at_current_position(self):
        string_io = _io.StringIO("hello world")
        string_io.seek(5)
        self.assertEqual(string_io.write("\n"), 1)
        self.assertEqual(string_io.getvalue(), "hello\nworld")

    def test_write_with_empty_string_does_nothing(self):
        string_io = _io.StringIO("hello world")
        string_io.seek(5)
        self.assertEqual(string_io.write(""), 0)
        self.assertEqual(string_io.getvalue(), "hello world")

    def test_write_with_larger_string_than_buffer_extends_buffer(self):
        string_io = _io.StringIO("hello")
        self.assertEqual(string_io.write("hello world"), 11)
        self.assertEqual(string_io.getvalue(), "hello world")

    def test_write_with_overseek_pads_end_of_buffer_to_position_with_zeros(self):
        string_io = _io.StringIO("hello")
        string_io.seek(7)
        self.assertEqual(string_io.write("world"), 5)
        self.assertEqual(string_io.getvalue(), "hello\0\0world")

    def test_write_with_write_translate_returns_original_size_of_string(self):
        string_io = _io.StringIO(newline=None)
        self.assertEqual(string_io.write("baz\r\n"), 5)
        self.assertEqual(string_io.getvalue(), "baz\n")

    def test_write_with_newline_none_translates_newlines_to_line_feed(self):
        string_io = _io.StringIO(newline=None)
        self.assertEqual(string_io.write("foo\n"), 4)
        self.assertEqual(string_io.write("bar\r"), 4)
        self.assertEqual(string_io.write("baz\r\n"), 5)
        self.assertEqual(string_io.getvalue(), "foo\nbar\nbaz\n")

    def test_write_with_newline_empty_doesnt_do_any_translations(self):
        string_io = _io.StringIO(newline="")
        self.assertEqual(string_io.write("foo\n"), 4)
        self.assertEqual(string_io.write("bar\r"), 4)
        self.assertEqual(string_io.write("baz\r\n"), 5)
        self.assertEqual(string_io.getvalue(), "foo\nbar\rbaz\r\n")

    def test_write_with_string_newline_translates_line_feed_to_string(self):
        string_io = _io.StringIO(newline="\r\n")
        self.assertEqual(string_io.write("foo\n"), 4)
        self.assertEqual(string_io.write("bar\r"), 4)
        self.assertEqual(string_io.write("baz\r\n"), 5)
        self.assertEqual(string_io.getvalue(), "foo\r\nbar\rbaz\r\r\n")

    def test_write_with_newline_none_stores_seen_newlines(self):
        string_io = _io.StringIO(newline=None)
        self.assertEqual(string_io.newlines, None)
        self.assertEqual(string_io.write("foo\n"), 4)
        self.assertEqual(string_io.newlines, "\n")
        self.assertEqual(string_io.write("bar\r"), 4)
        self.assertEqual(string_io.newlines, ("\r", "\n"))
        self.assertEqual(string_io.write("baz\r\n"), 5)
        self.assertEqual(string_io.newlines, ("\r", "\n", "\r\n"))
        self.assertEqual(string_io.getvalue(), "foo\nbar\nbaz\n")

    def test_writable_with_open_StringIO_returns_true(self):
        string_io = _io.StringIO()
        self.assertTrue(string_io.writable())

    def test_writable_with_closed_StringIO_raises_value_error(self):
        string_io = _io.StringIO()
        string_io.close()
        with self.assertRaises(ValueError) as context:
            string_io.writable()
        self.assertRegex(str(context.exception), "I/O operation on closed file")


if __name__ == "__main__":
    unittest.main()
