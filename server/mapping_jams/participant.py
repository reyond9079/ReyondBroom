"""Local account recovery and canonical avatar encoding."""

import io
import warnings

from PIL import Image, ImageOps, UnidentifiedImageError

MAX_AVATAR = 4 * 1024 * 1024
AVATAR_EDGE = 128
REPORT_REASONS = ("inappropriate", "broken_map", "spam", "other")


def avatar_bytes(data):
    if not 0 < len(data) <= MAX_AVATAR:
        raise ValueError("Choose an image under 4 MiB.")
    try:
        with warnings.catch_warnings():
            warnings.simplefilter("error", Image.DecompressionBombWarning)
            with Image.open(io.BytesIO(data), formats=("PNG", "JPEG", "WEBP")) as source:
                if (not 0 < source.width <= 4096 or not 0 < source.height <= 4096
                        or getattr(source, "n_frames", 1) != 1):
                    raise ValueError("Choose a still image no larger than 4096 × 4096.")
                fitted = ImageOps.fit(ImageOps.exif_transpose(source).convert("RGBA"),
                                      (AVATAR_EDGE, AVATAR_EDGE), Image.Resampling.LANCZOS)
                # A fresh image drops source metadata, including EXIF and color profiles.
                avatar = Image.frombytes("RGBA", fitted.size, fitted.tobytes())
                output = io.BytesIO()
                avatar.save(output, format="PNG")
                return output.getvalue()
    except (OSError, UnidentifiedImageError, Image.DecompressionBombWarning,
            Image.DecompressionBombError, SyntaxError) as error:
        raise ValueError("Choose a valid PNG, JPEG or WebP image.") from error
