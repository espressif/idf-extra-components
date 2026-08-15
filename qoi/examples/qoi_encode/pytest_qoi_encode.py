# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Unlicense OR CC0-1.0

import base64
import hashlib
import logging
import re
from dataclasses import dataclass
from pathlib import Path

import pytest
from pytest_embedded import Dut

QOI_META_PATTERN = (
    r'QOI_META width=(?P<width>\d+) height=(?P<height>\d+) '
    r'channels=(?P<channels>\d+) colorspace=(?P<colorspace>\d+) '
    r'encoding=(?P<encoding>\w+) size=(?P<size>\d+)'
)
QOI_META_RE = re.compile(QOI_META_PATTERN)
QOI_CHUNK_PATTERN = r'QOI_BASE64 (?P<payload>[A-Za-z0-9+/=]+)'
QOI_CHUNK_RE = re.compile(QOI_CHUNK_PATTERN)
QOI_OUTPUT_NAME = 'qoi_encode_result.qoi'
GOLDEN_RESULT_PATH = Path(__file__).with_name('golden_result.qoi')

EXPECTED_WIDTH = 160
EXPECTED_HEIGHT = 120
EXPECTED_CHANNELS = 4
EXPECTED_COLORSPACE = 0
EXPECTED_ENCODING = 'base64'


@dataclass(frozen=True)
class QoiMetadata:
    width: int
    height: int
    channels: int
    colorspace: int
    encoding: str
    size: int


def parse_qoi_metadata(meta_line: str) -> QoiMetadata:
    match = QOI_META_RE.fullmatch(meta_line)
    if not match:
        raise ValueError('Invalid QOI metadata line: {}'.format(meta_line))

    return QoiMetadata(
        width=int(match.group('width')),
        height=int(match.group('height')),
        channels=int(match.group('channels')),
        colorspace=int(match.group('colorspace')),
        encoding=match.group('encoding'),
        size=int(match.group('size')),
    )


def collect_base64_payload(dut: Dut) -> list:
    payload_chunks = []
    while True:
        match = dut.expect(rf'(?P<line>QOI_BASE64_END|{QOI_CHUNK_PATTERN}\r?\n)')
        line = match.group('line').decode('utf-8').strip()
        if line == 'QOI_BASE64_END':
            return payload_chunks

        chunk_match = QOI_CHUNK_RE.fullmatch(line)
        assert chunk_match is not None
        payload_chunks.append(chunk_match.group('payload'))


def validate_qoi_metadata(metadata: QoiMetadata) -> None:
    if metadata.width != EXPECTED_WIDTH or metadata.height != EXPECTED_HEIGHT:
        raise ValueError('Expected {}x{} QOI, got {}x{}'.format(
            EXPECTED_WIDTH, EXPECTED_HEIGHT, metadata.width, metadata.height))
    if metadata.channels != EXPECTED_CHANNELS:
        raise ValueError('Expected {} channels, got {}'.format(EXPECTED_CHANNELS, metadata.channels))
    if metadata.colorspace != EXPECTED_COLORSPACE:
        raise ValueError('Expected colorspace {}, got {}'.format(EXPECTED_COLORSPACE, metadata.colorspace))
    if metadata.encoding != EXPECTED_ENCODING:
        raise ValueError('Unsupported payload encoding: {}'.format(metadata.encoding))


def decode_qoi_base64_payload(metadata: QoiMetadata, base64_chunks: list) -> bytes:
    validate_qoi_metadata(metadata)

    qoi_bytes = base64.b64decode(''.join(base64_chunks), validate=True)
    if len(qoi_bytes) != metadata.size:
        raise ValueError('Expected {} QOI bytes, got {}'.format(metadata.size, len(qoi_bytes)))

    return qoi_bytes


def save_qoi_artifact(qoi_bytes: bytes, output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    try:
        output_path.write_bytes(qoi_bytes)
    except OSError:
        logging.exception('Failed to save QOI artifact to %s', output_path)
        return

    logging.info('Saved QOI artifact to %s', output_path)


def verify_qoi_against_golden(actual_path: Path, golden_path: Path) -> None:
    expected_hash = hashlib.sha256(golden_path.read_bytes()).hexdigest()
    actual_hash = hashlib.sha256(actual_path.read_bytes()).hexdigest()
    assert actual_hash == expected_hash, (
        'QOI SHA-256 does not match golden result: '
        'expected {}, got {}'.format(expected_hash, actual_hash)
    )


@pytest.mark.generic
@pytest.mark.parametrize('target', ['esp32', 'esp32s3', 'esp32c3'], indirect=['target'])
def test_qoi_encode_example_generic(dut: Dut) -> None:
    dut.expect(r'Generated \d+x\d+ RGBA framebuffer: \d+ bytes')
    dut.expect(r'Encoded QOI size: \d+ bytes \(ratio .*%\)')

    metadata_line = dut.expect(QOI_META_PATTERN).group(0).decode('utf-8')
    metadata = parse_qoi_metadata(metadata_line)

    dut.expect_exact('QOI_BASE64_BEGIN')
    base64_chunks = collect_base64_payload(dut)

    qoi_bytes = decode_qoi_base64_payload(metadata, base64_chunks)
    output_path = Path(dut.logdir) / QOI_OUTPUT_NAME
    save_qoi_artifact(qoi_bytes, output_path)
    verify_qoi_against_golden(output_path, GOLDEN_RESULT_PATH)

    dut.expect_exact('QOI encode demo done.')
