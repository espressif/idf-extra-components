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
from pytest_embedded_idf.utils import idf_parametrize

# The example dumps the captured framebuffer between FRAMEBUFFER markers:
#   FRAMEBUFFER_BEGIN 240 240 BGR3
#   FB_BASE64 <base64 payload chunk>
#   ...
#   FRAMEBUFFER_END
FRAMEBUFFER_META_PATTERN = r'FRAMEBUFFER_BEGIN (?P<width>\d+) (?P<height>\d+) (?P<fourcc>\S+)'
FRAMEBUFFER_META_RE = re.compile(FRAMEBUFFER_META_PATTERN)
FRAMEBUFFER_CHUNK_PATTERN = r'FB_BASE64 (?P<payload>[A-Za-z0-9+/=]+)'
FRAMEBUFFER_CHUNK_RE = re.compile(FRAMEBUFFER_CHUNK_PATTERN)
FRAMEBUFFER_END = 'FRAMEBUFFER_END'

IMAGE_OUTPUT_NAME = 'lvgl_screenshot_result.ppm'
GOLDEN_IMAGE_NAME = 'golden_result.ppm'
EXPECTED_WIDTH = 240
EXPECTED_HEIGHT = 240
EXPECTED_FOURCC = 'BGR3'
RGB888_BYTES_PER_PIXEL = 3
PPM_MAGIC = b'P6'
PPM_MAX_VALUE = b'255'
PPM_HEADER_RE = re.compile(rb'^P6\s+(?P<width>\d+)\s+(?P<height>\d+)\s+(?P<max_value>\d+)\s')
FOURCC_BYTES_PER_PIXEL = {
    'RGBL': 2,
    'RGBE': 2,
    'RGB3': 3,
    'BGR3': 3,
    'BA24': 4,
}


@dataclass(frozen=True)
class ImageMetadata:
    width: int
    height: int
    fourcc: str

    @property
    def framebuffer_size(self) -> int:
        try:
            bytes_per_pixel = FOURCC_BYTES_PER_PIXEL[self.fourcc]
        except KeyError as exc:
            raise ValueError(f'Unsupported framebuffer fourcc: {self.fourcc}') from exc
        return self.width * self.height * bytes_per_pixel


@dataclass(frozen=True)
class RgbImage:
    width: int
    height: int
    pixels_rgb888: bytes

    def __post_init__(self) -> None:
        expected_size = self.width * self.height * RGB888_BYTES_PER_PIXEL
        if len(self.pixels_rgb888) != expected_size:
            raise ValueError(f'Expected {expected_size} RGB bytes, got {len(self.pixels_rgb888)}')


def parse_image_metadata(meta_line: str) -> ImageMetadata:
    match = FRAMEBUFFER_META_RE.fullmatch(meta_line)
    if not match:
        raise ValueError(f'Invalid framebuffer metadata line: {meta_line}')

    return ImageMetadata(
        width=int(match.group('width')),
        height=int(match.group('height')),
        fourcc=match.group('fourcc'),
    )


def collect_base64_payload(dut: Dut) -> list[str]:
    payload_lines: list[str] = []
    while True:
        match = dut.expect(rf'(?P<line>{FRAMEBUFFER_END}|{FRAMEBUFFER_CHUNK_PATTERN}\r?\n)')
        line = match.group('line').decode('utf-8').strip()
        if line == FRAMEBUFFER_END:
            return payload_lines

        chunk_match = FRAMEBUFFER_CHUNK_RE.fullmatch(line)
        assert chunk_match is not None
        payload_lines.append(chunk_match.group('payload'))


def _framebuffer_to_rgb888(fourcc: str, framebuffer: bytes) -> bytes:
    if fourcc == 'RGB3':
        return framebuffer
    if fourcc == 'BGR3':
        pixels = bytearray(framebuffer)
        pixels[0::3], pixels[2::3] = pixels[2::3], pixels[0::3]
        return bytes(pixels)
    raise ValueError(f'Unsupported framebuffer fourcc: {fourcc}')


def _encode_ppm(image: RgbImage) -> bytes:
    header = b'%s\n%d %d\n%s\n' % (PPM_MAGIC, image.width, image.height, PPM_MAX_VALUE)
    return header + image.pixels_rgb888


def _load_ppm(path: Path) -> RgbImage:
    ppm_bytes = path.read_bytes()
    header_match = PPM_HEADER_RE.match(ppm_bytes)
    if not header_match:
        raise ValueError(f'{path} is not a supported binary PPM (P6, maxval 255) file')

    width = int(header_match.group('width'))
    height = int(header_match.group('height'))
    max_value = header_match.group('max_value')
    if width <= 0 or height <= 0:
        raise ValueError(f'Unsupported PPM dimensions in {path}')
    if max_value != PPM_MAX_VALUE:
        raise ValueError(f'Unsupported PPM max value in {path}: {max_value!r}')

    pixel_data = ppm_bytes[header_match.end() :]
    expected_size = width * height * RGB888_BYTES_PER_PIXEL
    if len(pixel_data) != expected_size:
        raise ValueError(f'Expected {expected_size} PPM pixel bytes in {path}, got {len(pixel_data)}')

    return RgbImage(width=width, height=height, pixels_rgb888=pixel_data)


def decode_framebuffer_image(metadata: ImageMetadata, payload_lines: list[str]) -> RgbImage:
    raw_bytes = base64.b64decode(''.join(payload_lines), validate=True)
    if len(raw_bytes) != metadata.framebuffer_size:
        raise ValueError(f'Expected {metadata.framebuffer_size} decoded bytes, got {len(raw_bytes)}')

    return RgbImage(
        width=metadata.width,
        height=metadata.height,
        pixels_rgb888=_framebuffer_to_rgb888(metadata.fourcc, raw_bytes),
    )


def save_ppm_artifact(image: RgbImage, output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    try:
        output_path.write_bytes(_encode_ppm(image))
    except OSError:
        logging.exception('Failed to save screenshot artifact to %s', output_path)
        return

    logging.info('Saved RGB888 image to %s', output_path)


def image_digest(image: RgbImage) -> str:
    digest = hashlib.sha256()
    digest.update(image.width.to_bytes(4, 'big'))
    digest.update(image.height.to_bytes(4, 'big'))
    digest.update(image.pixels_rgb888)
    return digest.hexdigest()


def assert_image_matches_golden(result_image: RgbImage, golden_path: Path) -> None:
    assert golden_path.is_file(), (
        f'Golden image {golden_path.name} not found. Run this test once, inspect '
        f'{IMAGE_OUTPUT_NAME} in the test log directory, and copy it next to this '
        f'pytest script as {GOLDEN_IMAGE_NAME}.'
    )
    golden_image = _load_ppm(golden_path)
    expected_digest = image_digest(golden_image)
    actual_digest = image_digest(result_image)
    assert actual_digest == expected_digest, (
        f'Rendered image does not match the golden image {golden_path.name}: '
        f'expected SHA-256 {expected_digest}, got {actual_digest}'
    )


@pytest.mark.generic
@idf_parametrize('target', ['esp32', 'esp32p4'], indirect=['target'])
def test_lvgl_screenshot_example_generic(dut: Dut) -> None:
    dut.expect(rf'Screenshot panel created \(\d+x\d+, {EXPECTED_FOURCC}, virtual only\)')
    dut.expect(r'PNG file size: \d+ bytes')

    metadata_line = dut.expect(FRAMEBUFFER_META_PATTERN).group(0).decode('utf-8').strip()
    metadata = parse_image_metadata(metadata_line)
    assert (metadata.width, metadata.height, metadata.fourcc) == (
        EXPECTED_WIDTH, EXPECTED_HEIGHT, EXPECTED_FOURCC
    ), f'Unexpected framebuffer geometry: {metadata.width}x{metadata.height} {metadata.fourcc}'

    result_image = decode_framebuffer_image(metadata, collect_base64_payload(dut))
    save_ppm_artifact(result_image, Path(dut.logdir) / IMAGE_OUTPUT_NAME)
    assert_image_matches_golden(result_image, Path(__file__).with_name(GOLDEN_IMAGE_NAME))

    dut.expect_exact('LVGL screenshot example done.')
