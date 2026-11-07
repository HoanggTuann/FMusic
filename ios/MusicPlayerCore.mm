#import "MusicPlayerCore.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "Player.h"

@implementation MPCTrackInfo
@end

@implementation MusicPlayerCore {
    std::unique_ptr<Player> _player;
    std::vector<float> _scratch;       // PCM interleaved tạm để tách kênh
    MPCTrackInfo *_info;
}

- (instancetype)init {
    if ((self = [super init])) {
        _player = std::make_unique<Player>();
        _scratch.resize(4096 * 2);
        _info = [MPCTrackInfo new];
        _info.title = @""; _info.artist = @""; _info.album = @"";
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
    const TrackInfo &ti = _player->info();
    MPCTrackInfo *info = [MPCTrackInfo new];
    info.title    = [NSString stringWithUTF8String:ti.title.c_str()]  ?: @"";
    info.artist   = [NSString stringWithUTF8String:ti.artist.c_str()] ?: @"";
    info.album    = [NSString stringWithUTF8String:ti.album.c_str()]  ?: @"";
    info.duration = ti.duration;
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

@end
