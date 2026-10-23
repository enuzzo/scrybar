#!/usr/bin/env python3
"""Test actual adapter metadata/retention source without a live MediaRemote feed."""
from pathlib import Path
import subprocess
import tempfile

vendor = Path(__file__).resolve().parents[1]
stream = (vendor / "src/adapter/stream.m").read_text()


def block_at(marker):
    start = stream.index(marker)
    opening = stream.index("{", start)
    depth = 1
    cursor = opening + 1
    while depth:
        depth += (stream[cursor] == "{") - (stream[cursor] == "}")
        cursor += 1
    return stream[start:cursor]


predicate = block_at("static BOOL isSameItemIdentity(")
retention = block_at("if (isSameItemIdentity(liveData, converted) &&")
source = r'''
#import <Foundation/Foundation.h>
#import "MediaRemoteAdapter.h"
#import "adapter/now_playing.h"
#import "adapter/globals.h"
#import "adapter/keys.h"
MediaRemote *g_mediaRemote = nil;
dispatch_queue_t g_serialdispatchQueue;
''' + predicate + r'''
static NSDictionary *retain(NSDictionary *liveData, NSMutableDictionary *converted) {
''' + retention + r'''
    return converted;
}
int main(void) {
  @autoreleasepool {
    NSDictionary *input = @{
      @"kMRMediaRemoteNowPlayingInfoTitle": @"Synthetic test",
      @"kMRMediaRemoteNowPlayingInfoArtworkIdentifier": @"synthetic-artwork-identity",
      @"kMRMediaRemoteNowPlayingInfoArtworkData": [NSData dataWithBytes:"abc" length:3],
      @"kMRMediaRemoteNowPlayingInfoDuration": @120.5,
      @"kMRMediaRemoteNowPlayingInfoTimestamp": [NSDate dateWithTimeIntervalSince1970:1234]
    };
    for (unsigned micros=0; micros<2; ++micros) {
      for (unsigned withoutArtwork=0; withoutArtwork<2; ++withoutArtwork) {
        NSDictionary *out = convertNowPlayingInformation(input, micros, false, withoutArtwork);
        NSCAssert([out[@"artworkIdentifier"] isEqual:input[@"kMRMediaRemoteNowPlayingInfoArtworkIdentifier"]], @"identity survives metadata modes");
        NSCAssert([out[@"title"] isEqual:@"Synthetic test"], @"title preserved");
        NSCAssert((out[@"artworkData"] == nil) == (withoutArtwork != 0), @"no-artwork honored");
        if (micros) NSCAssert([out[@"timestampEpochMicros"] longLongValue] == 1234000000, @"microsecond epoch preserved");
      }
    }
    NSMutableDictionary *withoutId = [input mutableCopy];
    [withoutId removeObjectForKey:@"kMRMediaRemoteNowPlayingInfoArtworkIdentifier"];
    NSCAssert(convertNowPlayingInformation(withoutId, true, false, false)[@"artworkIdentifier"] == nil, @"absent optional identity omitted");
    NSCAssert(convertNowPlayingInformation(nil, true, false, false)[@"artworkIdentifier"] == nil, @"nil metadata remains empty");

    NSArray *expectedKeys = @[@"processIdentifier", @"bundleIdentifier", @"parentApplicationBundleIdentifier", @"title", @"artist", @"album", @"uniqueIdentifier", @"contentItemIdentifier", @"artworkIdentifier"];
    NSCAssert([[NSSet setWithArray:identifyingPayloadKeys()] isEqual:[NSSet setWithArray:expectedKeys]], @"all nine identity keys participate");
    NSMutableDictionary *old = [NSMutableDictionary dictionary];
    for (NSString *key in expectedKeys) old[key] = @"same";
    NSData *oldPixels = [NSData dataWithBytes:"old" length:3];
    old[@"artworkData"] = oldPixels;
    old[@"artworkMimeType"] = @"image/jpeg";
    NSMutableDictionary *next = [old mutableCopy];
    [next removeObjectForKey:@"artworkData"];
    [next removeObjectForKey:@"artworkMimeType"];
    NSCAssert([retain(old, [next mutableCopy])[@"artworkData"] isEqual:oldPixels], @"same identity retains pixels");
    NSCAssert([retain(old, [next mutableCopy])[@"artworkMimeType"] isEqual:@"image/jpeg"], @"matching MIME retained");
    for (NSString *key in expectedKeys) {
      NSMutableDictionary *changed = [next mutableCopy];
      changed[key] = @"changed";
      NSCAssert(retain(old, changed)[@"artworkData"] == nil, @"changed identity cannot inherit old pixels");
      NSMutableDictionary *missing = [next mutableCopy];
      [missing removeObjectForKey:key];
      NSCAssert(retain(old, missing)[@"artworkData"] == nil, @"missing known identity cannot inherit old pixels");
    }
    NSMutableDictionary *legacyOld = [old mutableCopy];
    NSMutableDictionary *legacyNext = [next mutableCopy];
    for (NSString *key in @[@"uniqueIdentifier", @"contentItemIdentifier", @"artworkIdentifier"]) {
      [legacyOld removeObjectForKey:key];
      [legacyNext removeObjectForKey:key];
    }
    NSCAssert([retain(legacyOld, legacyNext)[@"artworkData"] isEqual:oldPixels], @"optional keys absent on both sides preserve legacy behavior");
    NSMutableDictionary *fresh = [next mutableCopy];
    NSData *freshPixels = [NSData dataWithBytes:"new" length:3];
    fresh[@"artworkData"] = freshPixels;
    NSCAssert([retain(old, fresh)[@"artworkData"] isEqual:freshPixels], @"fresh pixels always honored");
    puts("actual-source metadata: 4 modes + absent/nil identity PASS; retention: 9 changed + 9 missing keys, same/legacy/fresh pixels PASS");
  }
  return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="scrybar-adapter-test-", dir="/tmp") as tmp:
    work = Path(tmp)
    harness = work / "metadata-test.m"
    harness.write_text(source)
    binary = work / "metadata-test"
    subprocess.run([
        "xcrun", "clang", "-fobjc-arc", "-fblocks", "-mmacosx-version-min=14.0",
        "-framework", "Foundation", "-framework", "AppKit",
        "-I", str(vendor / "include"), "-I", str(vendor / "src"), str(harness),
        str(vendor / "src/adapter/now_playing.m"),
        str(vendor / "src/adapter/keys.m"),
        str(vendor / "src/private/MediaRemote.m"), "-o", str(binary),
    ], check=True)
    # globals.m (which initializes MediaRemote) is deliberately not linked.
    subprocess.run([str(binary)], check=True)
