#import "MusicPlayerCore.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "AudioDecoder.h"
#include "Equalizer.h"
#include "Lyrics.h"
#include "Player.h"

@implementation MPCTrackInfo
@end
@implementation MPCLyricLine
@end
@implementation MPCLyrics
@end

static NSString *NS(const std::string &s) {
    return [NSString stringWithUTF8String:s.c_str()] ?: @"";
}

static MPCTrackInfo *MakeInfo(const TrackInfo &ti) {
    MPCTrackInfo *info = [MPCTrackInfo new];
    info.title = NS(ti.title);
    info.artist = NS(ti.artist);
    info.album = NS(ti.album);
    info.lyrics = NS(ti.lyrics);
    info.duration = ti.duration;
    return info;
}

@implementation MusicPlayerCore {
    std::unique_ptr<Player> _player;
    std::vector<float> _scratch;       // PCM interleaved tạm để tách kênh
    MPCTrackInfo *_info;
}

- (instancetype)init {
    if ((self = [super init])) {
        _player = std::make_unique<Player>();
        _scratch.resize(4096 * 2);
        _info = MakeInfo(TrackInfo());
    }
    return self;
}

- (BOOL)loadPath:(NSString *)path error:(NSError **)error {
    std::string err;
    if (!_player->load(path.UTF8String, &err)) {
        if (error) {
            NSString *msg = [NSString stringWithUTF8String:err.c_str()] ?: @"Không mở được tệp";
            *error = [NSError errorWithDomain:@"MusicPlayerCore" code:1
                                     userInfo:@{NSLocalizedDescriptionKey: msg}];
        }
        return NO;
    }
    MPCTrackInfo *info = MakeInfo(_player->info());
    const auto &c = _player->coverArt();
    info.coverArt = c.empty() ? nil : [NSData dataWithBytes:c.data() length:c.size()];
    _info = info;
    return YES;
}

- (void)play  { _player->play(); }
- (void)pause { _player->pause(); }
- (void)seekToSeconds:(double)seconds { _player->seek(seconds); }

- (BOOL)playing { return _player->playing(); }
- (BOOL)ended   { return _player->ended(); }
- (double)position { return _player->position(); }
- (double)duration { return _player->duration(); }
- (NSInteger)bandCount { return Player::kBands; }
- (MPCTrackInfo *)trackInfo { return _info; }

- (void)pullLeft:(float *)left right:(float *)right frames:(NSInteger)frames {
    size_t done = 0;
    const size_t total = (size_t)frames;
    while (done < total) {
        const size_t n = std::min<size_t>(4096, total - done);
        _player->pull(_scratch.data(), n);
        for (size_t i = 0; i < n; ++i) {            // interleaved -> planar
            left[done + i]  = _scratch[2 * i];
            right[done + i] = _scratch[2 * i + 1];
        }
        done += n;
    }
}

- (float)copySpectrum:(float *)bands count:(NSInteger)count {
    float tmp[Player::kBands];
    const float bass = _player->spectrum(tmp);
    const NSInteger n = std::min<NSInteger>(count, Player::kBands);
    for (NSInteger i = 0; i < n; ++i) bands[i] = tmp[i];
    return bass;
}

// ----- Equalizer -----
- (BOOL)eqEnabled { return _player->eq().enabled(); }
- (void)setEqEnabled:(BOOL)on { _player->eq().setEnabled(on); }
- (void)setEqGain:(float)db forBand:(NSInteger)band { _player->eq().setGain((int)band, db); }
- (float)eqGainForBand:(NSInteger)band { return _player->eq().gain((int)band); }
- (void)applyEqPreset:(NSInteger)index { _player->eq().applyPreset((int)index); }

+ (NSArray<NSString *> *)eqPresetNames {
    NSMutableArray *names = [NSMutableArray array];
    for (int i = 0; i < Equalizer::kPresetCount; ++i) [names addObject:NS(Equalizer::presetName(i))];
    return names;
}

// ----- Tiện ích tĩnh -----
+ (MPCTrackInfo *)probePath:(NSString *)path {
    TrackInfo ti;
    if (!AudioDecoder::probe(path.UTF8String, ti)) return nil;
    return MakeInfo(ti);
}

+ (MPCLyrics *)parseLyrics:(NSString *)text {
    Lyrics ly;
    ly.parse(text.UTF8String ?: "");
    NSMutableArray<MPCLyricLine *> *arr = [NSMutableArray arrayWithCapacity:ly.lines().size()];
    for (const LyricLine &l : ly.lines()) {
        MPCLyricLine *line = [MPCLyricLine new];
        line.time = l.time;
        line.text = NS(l.text);
        [arr addObject:line];
    }
    MPCLyrics *out = [MPCLyrics new];
    out.synced = ly.synced();
    out.lines = arr;
    return out;
}

@end
