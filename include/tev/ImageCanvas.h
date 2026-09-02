// This file was developed by Thomas Müller <thomas94@gmx.net>.
// It is published under the BSD 3-Clause License within the LICENSE file.

#pragma once

#include <tev/FlipMetric.h>
#include <tev/UberShader.h>
#include <tev/Image.h>
#include <tev/Lazy.h>

#include <nanogui/canvas.h>

#include <memory>

namespace tev {

struct CanvasStatistics {
    float mean;
    float maximum;
    float minimum;
    float meanSquaredError = 0.0f;
    float peakSignalToNoiseRatio = 0.0f;
    std::vector<float> histogram;
    int nChannels;
    int histogramZero;
    bool hasErrorMetrics = false;
    bool hasFlipError = false;
    bool flipUsesHdr = false;
    float flipMeanError = 0.0f;
    float flipPixelsPerDegree = 0.0f;
    float flipStartExposure = 0.0f;
    float flipStopExposure = 0.0f;
    int flipNumExposures = 0;
    std::string flipTonemapper;
    std::vector<float> flipErrorMap;
    std::vector<float> flipMagmaMapSrgb;
};

class ImageCanvas : public nanogui::Canvas {
public:
    ImageCanvas(nanogui::Widget* parent, float pixelRatio);

    bool scroll_event(const nanogui::Vector2i& p, const nanogui::Vector2f& rel) override;

    void draw_contents() override;

    void draw(NVGcontext *ctx) override;

    void translate(const nanogui::Vector2f& amount);
    void scale(float amount, const nanogui::Vector2f& origin);
    float scale() const {
        return nanogui::extractScale(mTransform);
    }

    void setExposure(float exposure) {
        mExposure = exposure;
    }

    void setOffset(float offset) {
        mOffset = offset;
    }

    void setGamma(float gamma) {
        mGamma = gamma;
    }

#if 1 // [DDS]
    bool getShowSRGB() const {
        return mShowSRGB;
    }
    void setShowSRGB(bool show_srgb) {
        mShowSRGB = show_srgb;
    }

    bool getShowHex() const {
        return mShowHex;
    }

    void setShowHex(bool show_hex) {
        mShowHex = show_hex;
    }

    bool getBitCastType() const {
        return mBitCastType;
    }

    void setBitCastType(EBitCastType bit_cast_type) {
        mBitCastType = bit_cast_type;
    }

    int extractMipLevel(const std::string& groupName) const;
#endif // [DDS]

    float applyExposureAndOffset(float value) const;

    void setImage(std::shared_ptr<Image> image) {
        mImage = image;
    }

    void setReference(std::shared_ptr<Image> reference) {
        mReference = reference;
    }

    void setRequestedChannelGroup(const std::string& groupName) {
        mRequestedChannelGroup = groupName;
    }

    nanogui::Vector2i getImageCoords(const Image& image, nanogui::Vector2i mousePos);

    void getValuesAtNanoPos(nanogui::Vector2i nanoPos, std::vector<float>& result, const std::vector<std::string>& channels);
    std::vector<float> getValuesAtNanoPos(nanogui::Vector2i nanoPos, const std::vector<std::string>& channels) {
        std::vector<float> result;
        getValuesAtNanoPos(nanoPos, result, channels);
        return result;
    }

    ETonemap tonemap() const {
        return mTonemap;
    }

    void setTonemap(ETonemap tonemap) {
        mTonemap = tonemap;
    }

    static nanogui::Vector3f applyTonemap(const nanogui::Vector3f& value, float gamma, ETonemap tonemap);
    nanogui::Vector3f applyTonemap(const nanogui::Vector3f& value) const {
        return applyTonemap(value, mGamma, mTonemap);
    }

    EMetric metric() const {
        return mMetric;
    }

    void setMetric(EMetric metric) {
        mMetric = metric;
    }

    EChannel channel() const {
        return mChannel;
    }

    void setChannel(EChannel channel) {
        mChannel = channel;
    }

    const nanogui::Vector2f& MinMax() const {
        return mMinMax;
    }

    void setMinMax(const nanogui::Vector2f& min_max) {
        mMinMax = min_max;
    }

    static float applyMetric(float value, float reference, EMetric metric);
    float applyMetric(float value, float reference) const {
        return applyMetric(value, reference, mMetric);
    }

    auto backgroundColor() {
        return mShader->backgroundColor();
    }

    void setBackgroundColor(const nanogui::Color& color) {
        mShader->setBackgroundColor(color);
    }

    void fitImageToScreen(const Image& image);
    void resetTransform();

    void setClipToLdr(bool value) {
        mClipToLdr = value;
    }

    bool clipToLdr() const {
        return mClipToLdr;
    }

    std::vector<float> getHdrImageData(bool divideAlpha, int priority) const;
    std::vector<char> getLdrImageData(bool divideAlpha, int priority) const;

#if 1 // [DDS]
    std::vector<char> getLdrImageDataClip() const;
#endif // [DDS]

    void saveImage(const fs::path& filename) const;

    std::shared_ptr<Lazy<std::shared_ptr<CanvasStatistics>>> canvasStatistics();

    void purgeCanvasStatistics(int imageId);

private:
    static FlipMetricResult computeFlipMetricForImages(
        const Image& image,
        const Image& reference,
        const std::string& requestedChannelGroup
    );

    std::shared_ptr<Image> flipDisplayImage(
        const std::string& key,
        const CanvasStatistics& statistics
    );

    static std::vector<Channel> channelsFromImages(
        std::shared_ptr<Image> image,
        std::shared_ptr<Image> reference,
        const std::string& requestedChannelGroup,
        EMetric metric,
        int priority
#if 1 // [DDS]
        , bool show_srgb
#endif // [DDS]
    );

    static Task<std::shared_ptr<CanvasStatistics>> computeCanvasStatistics(
        std::shared_ptr<Image> image,
        std::shared_ptr<Image> reference,
        const std::string& requestedChannelGroup,
        EMetric metric,
        int priority
#if 1 // [DDS]
        , bool show_srgb
        , EChannel channel_mask
#endif // [DDS]
    );

    void drawPixelValuesAsText(NVGcontext *ctx);
    void drawCoordinateSystem(NVGcontext *ctx);
    void drawEdgeShadows(NVGcontext *ctx);

    nanogui::Vector2f pixelOffset(const nanogui::Vector2i& size) const;

    // Assembles the transform from canonical space to
    // the [-1, 1] square for the current image.
    nanogui::Matrix3f transform(const Image* image);
    nanogui::Matrix3f textureToNanogui(const Image* image);
    nanogui::Matrix3f displayWindowToNanogui(const Image* image);

    float mPixelRatio = 1;
    float mExposure = 0;
    float mOffset = 0;

#if 0 // [DDS]
    float mGamma = 2.2f;
#else
    float mGamma = 1.0f;
#endif // [DDS]

#if 1 // [DDS]
    bool mShowSRGB = false;
    bool mShowHex = true;
    EBitCastType mBitCastType = EBitCastType::Float;
    nanogui::Vector2i mNanoPos;
    std::vector<float> mValuesAtNanoPos;
#endif // [DDS]

    bool mClipToLdr = false;

    std::shared_ptr<Image> mImage;
    std::shared_ptr<Image> mReference;

    std::string mRequestedChannelGroup = "";

    nanogui::Matrix3f mTransform = nanogui::Matrix3f::scale(nanogui::Vector3f(1.0f));

    std::unique_ptr<UberShader> mShader;

    ETonemap mTonemap = SRGB;
    EMetric mMetric = Error;
    EChannel mChannel = ChannelRGB;
    nanogui::Vector2f mMinMax = { 0, 1 };

    std::map<std::string, std::shared_ptr<Lazy<std::shared_ptr<CanvasStatistics>>>> mCanvasStatistics;
    std::map<int, std::vector<std::string>> mImageIdToCanvasStatisticsKey;
    std::map<std::string, std::shared_ptr<Image>> mFlipDisplayImages;
};

}
