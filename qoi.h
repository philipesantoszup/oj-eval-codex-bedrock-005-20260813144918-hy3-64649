#ifndef QOI_FORMAT_CODEC_QOI_H_
#define QOI_FORMAT_CODEC_QOI_H_

#include "utils.h"

constexpr uint8_t QOI_OP_INDEX_TAG = 0x00;
constexpr uint8_t QOI_OP_DIFF_TAG  = 0x40;
constexpr uint8_t QOI_OP_LUMA_TAG  = 0x80;
constexpr uint8_t QOI_OP_RUN_TAG   = 0xc0; 
constexpr uint8_t QOI_OP_RGB_TAG   = 0xfe;
constexpr uint8_t QOI_OP_RGBA_TAG  = 0xff;
constexpr uint8_t QOI_PADDING[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u};
constexpr uint8_t QOI_MASK_2 = 0xc0;

/**
 * @brief encode the raw pixel data of an image to qoi format.
 *
 * @param[in] width image width in pixels
 * @param[in] height image height in pixels
 * @param[in] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[in] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace = 0);

/**
 * @brief decode the qoi format of an image to raw pixel data
 *
 * @param[out] width image width in pixels
 * @param[out] height image height in pixels
 * @param[out] channels number of color channels, 3 = RGB, 4 = RGBA
 * @param[out] colorspace image color space, 0 = sRGB with linear alpha, 1 = all channels linear
 *
 * @return bool true if it is a valid qoi format image, false otherwise
 */
bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace);


bool QoiEncode(uint32_t width, uint32_t height, uint8_t channels, uint8_t colorspace) {

    // qoi-header part

    // write magic bytes "qoif"
    QoiWriteChar('q');
    QoiWriteChar('o');
    QoiWriteChar('i');
    QoiWriteChar('f');
    // write image width
    QoiWriteU32(width);
    // write image height
    QoiWriteU32(height);
    // write channel number
    QoiWriteU8(channels);
    // write color space specifier
    QoiWriteU8(colorspace);

    /* qoi-data part */
    uint64_t px_num = static_cast<uint64_t>(width) * height;

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    int run = 0;
    uint8_t r = 0u, g = 0u, b = 0u, a = 255u;
    uint8_t pre_r = 0u, pre_g = 0u, pre_b = 0u, pre_a = 255u;

    for (uint64_t i = 0; i < px_num; ++i) {
        r = QoiReadU8();
        g = QoiReadU8();
        b = QoiReadU8();
        if (channels == 4) a = QoiReadU8();

        if (r == pre_r && g == pre_g && b == pre_b && a == pre_a) {
            ++run;
            if (run == 62) {
                QoiWriteU8(QOI_OP_RUN_TAG | static_cast<uint8_t>(run - 1));
                run = 0;
            }
        } else {
            if (run > 0) {
                QoiWriteU8(QOI_OP_RUN_TAG | static_cast<uint8_t>(run - 1));
                run = 0;
            }

            int index_pos = QoiColorHash(r, g, b, a);
            if (history[index_pos][0] == r && history[index_pos][1] == g &&
                history[index_pos][2] == b && history[index_pos][3] == a) {
                QoiWriteU8(QOI_OP_INDEX_TAG | static_cast<uint8_t>(index_pos));
            } else {
                history[index_pos][0] = r;
                history[index_pos][1] = g;
                history[index_pos][2] = b;
                history[index_pos][3] = a;

                if (a == pre_a) {
                    int8_t vr = static_cast<int8_t>(r) - static_cast<int8_t>(pre_r);
                    int8_t vg = static_cast<int8_t>(g) - static_cast<int8_t>(pre_g);
                    int8_t vb = static_cast<int8_t>(b) - static_cast<int8_t>(pre_b);
                    int8_t vg_r = vr - vg;
                    int8_t vg_b = vb - vg;

                    if (vr > -3 && vr < 2 && vg > -3 && vg < 2 && vb > -3 && vb < 2) {
                        QoiWriteU8(QOI_OP_DIFF_TAG |
                                   static_cast<uint8_t>((vr + 2) << 4) |
                                   static_cast<uint8_t>((vg + 2) << 2) |
                                   static_cast<uint8_t>(vb + 2));
                    } else if (vg_r > -9 && vg_r < 8 && vg > -33 && vg < 32 && vg_b > -9 && vg_b < 8) {
                        QoiWriteU8(QOI_OP_LUMA_TAG | static_cast<uint8_t>(vg + 32));
                        QoiWriteU8(static_cast<uint8_t>(((vg_r + 8) << 4) | (vg_b + 8)));
                    } else {
                        QoiWriteU8(QOI_OP_RGB_TAG);
                        QoiWriteU8(r);
                        QoiWriteU8(g);
                        QoiWriteU8(b);
                    }
                } else {
                    QoiWriteU8(QOI_OP_RGBA_TAG);
                    QoiWriteU8(r);
                    QoiWriteU8(g);
                    QoiWriteU8(b);
                    QoiWriteU8(a);
                }
            }
        }

        pre_r = r;
        pre_g = g;
        pre_b = b;
        pre_a = a;
    }
    if (run > 0) {
        QoiWriteU8(QOI_OP_RUN_TAG | static_cast<uint8_t>(run - 1));
    }

    // qoi-padding part
    for (int i = 0; i < sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0]); ++i) {
        QoiWriteU8(QOI_PADDING[i]);
    }

    return true;
}

bool QoiDecode(uint32_t &width, uint32_t &height, uint8_t &channels, uint8_t &colorspace) {

    char c1 = QoiReadChar();
    char c2 = QoiReadChar();
    char c3 = QoiReadChar();
    char c4 = QoiReadChar();
    if (c1 != 'q' || c2 != 'o' || c3 != 'i' || c4 != 'f') {
        return false;
    }

    // read image width
    width = QoiReadU32();
    // read image height
    height = QoiReadU32();
    // read channel number
    channels = QoiReadU8();
    // read color space specifier
    colorspace = QoiReadU8();

    if (channels != 3 && channels != 4) {
        return false;
    }

    uint64_t px_num = static_cast<uint64_t>(width) * height;

    uint8_t history[64][4];
    memset(history, 0, sizeof(history));

    int run = 0;
    uint8_t r = 0u, g = 0u, b = 0u, a = 255u;

    for (uint64_t i = 0; i < px_num; ++i) {
        if (run > 0) {
            --run;
        } else {
            uint8_t byte = QoiReadU8();
            if (byte == QOI_OP_RGB_TAG) {
                r = QoiReadU8();
                g = QoiReadU8();
                b = QoiReadU8();
            } else if (byte == QOI_OP_RGBA_TAG) {
                r = QoiReadU8();
                g = QoiReadU8();
                b = QoiReadU8();
                a = QoiReadU8();
            } else if ((byte & QOI_MASK_2) == QOI_OP_INDEX_TAG) {
                int index_pos = byte & 0x3f;
                r = history[index_pos][0];
                g = history[index_pos][1];
                b = history[index_pos][2];
                a = history[index_pos][3];
            } else if ((byte & QOI_MASK_2) == QOI_OP_DIFF_TAG) {
                r = static_cast<uint8_t>(r + ((byte >> 4) & 3) - 2);
                g = static_cast<uint8_t>(g + ((byte >> 2) & 3) - 2);
                b = static_cast<uint8_t>(b + (byte & 3) - 2);
            } else if ((byte & QOI_MASK_2) == QOI_OP_LUMA_TAG) {
                uint8_t byte2 = QoiReadU8();
                int vg = static_cast<int>(byte & 0x3f) - 32;
                int vg_r = static_cast<int>((byte2 >> 4) & 0xf) - 8;
                int vg_b = static_cast<int>(byte2 & 0xf) - 8;
                r = static_cast<uint8_t>(r + vg_r + vg);
                g = static_cast<uint8_t>(g + vg);
                b = static_cast<uint8_t>(b + vg_b + vg);
            } else if ((byte & QOI_MASK_2) == QOI_OP_RUN_TAG) {
                run = byte & 0x3f;
            }
        }

        int index_pos = QoiColorHash(r, g, b, a);
        history[index_pos][0] = r;
        history[index_pos][1] = g;
        history[index_pos][2] = b;
        history[index_pos][3] = a;

        QoiWriteU8(r);
        QoiWriteU8(g);
        QoiWriteU8(b);
        if (channels == 4) QoiWriteU8(a);
    }

    bool valid = true;
    for (int i = 0; i < sizeof(QOI_PADDING) / sizeof(QOI_PADDING[0]); ++i) {
        if (QoiReadU8() != QOI_PADDING[i]) valid = false;
    }

    return valid;
}

#endif // QOI_FORMAT_CODEC_QOI_H_
