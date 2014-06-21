#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

// Header:
typedef struct __attribute__((__packed__)) {
  uint16_t magic;      // Seems to always be 20 05.
  uint8_t version[4];  // Watch software version.
  uint16_t unkown1;
  uint32_t timestamp;  // Seconds since 1/1/1970
  uint8_t unknown2[105];
} Header;

// Tag 0x22
typedef struct __attribute__((__packed__)) {
  int32_t latitude;  // in 1e-7 degrees
  int32_t longitude; // in 1e-7 degrees
  uint16_t u1;
  uint16_t speed;  // 100 * m/s
  uint16_t u2;
  uint32_t time; // seconds since 1970
  uint32_t calories;
  uint16_t u3;
  uint16_t distance;  // in .1 m
  uint8_t u4;
} GPS;

// Tag 0x25
typedef struct __attribute__((__packed__)) {
  uint16_t heart_rate;
  uint32_t time;
} HeartRate;

typedef struct __attribute__((__packed__)) {
  uint8_t lap;
  uint8_t activity;
  uint32_t time;
} Lap;

typedef struct __attribute__((__packed__)) {
  uint8_t u[2];
  uint32_t time;
} UnknownAndTime;

// Tag 0x27
typedef struct __attribute__((__packed__)) {
  uint32_t activity_type;  // 7 = treadmill?
  uint32_t distance;  // meters.
  uint32_t duration;  // seconds (add 1).
  uint32_t calories;
} Summary;

// Tag 0x32
typedef struct __attribute__((__packed__)) {
  uint32_t time;  // seconds since 1970
  float distance; // meters
  uint32_t calories;
  uint32_t steps;  // steps?
  uint16_t u2;
} Treadmill;

// Tag 0x23 ??
typedef struct __attribute__((__packed__)) {
  uint16_t u1;
  uint16_t u2;
  uint8_t u3;
  uint8_t u4[4];
  uint8_t u5[4];
  uint16_t u6;
  uint8_t u7[4];
} R23;

typedef struct __attribute__((__packed__)) {
  uint32_t time;  // Seconds since 1/1/1970
  uint8_t u[14];
  uint32_t calories;
} Swim;

void ReadStruct(FILE* f, void* data, int size) {
  if (fread(data, 1, size, f) != size) {
    perror("Failed to read the file");
    exit(-1);
  }
}

// Activities:
// 0: Run
// 1: Cycle
// 2: Swim
// 7: Treadmill

const char* GetActivityType(uint8_t activity) {
  static char type[128];
  switch(activity) {
    case 0: return "Run";
    case 1: return "Cycle";
    case 2: return "Swim";
    case 7: return "Treadmill";
    default:
      snprintf(type, sizeof(type), "Type %i", activity);
      return type;
  }
}

const char* GetGMTTime(uint32_t seconds) {
  time_t tt = seconds;
  static char buffer[128];
  if (strftime(buffer, sizeof(buffer), "%F %T", gmtime(&tt)) > 0) {
    return buffer;
  }
  return "";
}

const char* GetLocalTime(uint32_t seconds) {
  time_t tt = seconds;
  static char buffer[128];
  if (strftime(buffer, sizeof(buffer), "%F %T", localtime(&tt)) > 0) {
    return buffer;
  }
  return "";
}

void ReadAndDump(int size, FILE* f) {
  uint8_t buffer[256];
  if (fread(buffer, 1, size, f) != size) {
    perror("Failed to read the file");
    exit(-1);
  }
  for (int i = 0; i < size; ++i) {
    printf(" %02X", buffer[i]);
    if (i % 32 == 31) {
      printf("\n");
    }
  }
  if (size % 32 != 0) {
    printf("\n");
  }
}

void Dump(const uint8_t* data, int size) {
  for (int i = 0; i < size; ++i) {
    printf(" %02X", data[i]);
    if (i % 32 == 31) {
      printf("\n");
    }
  }
  if (size % 32 != 0) {
    printf("\n");
  }
}

int main(int argc, char** argv) {
  if (argc < 2) {
    printf("Need the filename.\n");
    return -1;
  }

  FILE* f = fopen(argv[1], "r");
  if (f == NULL) {
    printf("Failed to open: %s\n", argv[1]);
    return -1;
  }
  // Skip the header (structure mostly unknown).
  Header header;
  ReadStruct(f, &header, sizeof(header));

  uint8_t buffer[255];

  GPS gps;
  HeartRate heart;
  Summary summary;
  Treadmill treadmill;
  Swim swim;
  Lap lap;
  UnknownAndTime r35;
  R23 r23;
  while(!feof(f)) {
    if (fread(buffer, 1, 1, f) < 1) {
      return 0;
    }
    uint8_t tag = buffer[0];
    switch(tag) {
      case 0x21:
        ReadStruct(f, &lap, sizeof(lap));
        printf("[%s] Lap: %i activity: %s\n", GetLocalTime(lap.time), lap.lap,
               GetActivityType(lap.activity));
        break;

      case 0x22:
        printf("\n");
        ReadStruct(f, &gps, sizeof(gps));
        if (gps.time != 0xffffffff) {
          printf("[%s] GPS: Lat: %f, Long: %f, Speed: %.2f m/s,"
                 "Cal: %i, Distance: %.1f m   "
                 "%04X %04X %04X %02X\n",
                 GetLocalTime(gps.time),
                 gps.latitude * 1e-7, gps.longitude * 1e-7, gps.speed * 0.01,
                 gps.calories,
                 gps.distance * 0.1,  gps.u1, gps.u2, gps.u3, gps.u4);
        } else {
          printf("No GPS lock\n");
        }
        printf("\n");
        break;

      case 0x23:
        ReadStruct(f, &r23, sizeof(r23));
        printf("Tag 0x23: %04X  %04X  %02X\n", r23.u1, r23.u2, r23.u3);
        Dump((const uint8_t*)&r23, sizeof(r23));
        break;

      case 0x25:
        ReadStruct(f, &heart, sizeof(heart));
        printf("[%s] Heart BPM: %i\n", GetGMTTime(heart.time),
               heart.heart_rate);
        break;

      case 0x26:
        printf("Tag 0x26: ");
        ReadAndDump(6, f);
        break;

      case 0x27:
        ReadStruct(f, &summary, sizeof(summary));
        printf("Summary:\n  Activity type: %s\n  Distance %im\n"
               "  Duration: %i s\n  Calories: %i\n",
               GetActivityType(summary.activity_type), summary.distance,
               summary.duration + 1, summary.calories);
        break;

      case 0x30:
        printf("Tag 0x30: ");
        ReadAndDump(2, f);
        break;

      case 0x32:
        ReadStruct(f, &treadmill, sizeof(Treadmill));
        printf("[%s] Treadmill: Distance: %.2f m  Calories: %i  Steps: %i\n",
               GetGMTTime(treadmill.time), treadmill.distance,
               treadmill.calories, treadmill.steps);
        break;

      case 0x34:
        ReadStruct(f, &swim, sizeof(swim));
        printf("Swim: %s Calories: %i\n", GetGMTTime(swim.time),
               swim.calories);
        for (int i = 0; i < sizeof(swim.u); ++i) {
          printf(" %02X", swim.u[i]);
        }
        printf("\n");
        break;


      case 0x35:
        ReadStruct(f, &r35, sizeof(r35));
        printf("Tag 0x35: %02X %02X  %s\n", r35.u[0], r35.u[1],
               GetLocalTime(r35.time));
        break;

      case 0x37:
        printf("Tag 0x37: ");
        ReadAndDump(1, f);
        break;

      default:
        printf("Unknow tag: %02X at %li\n", tag, ftell(f) - 1);
        break;
    }
  };

  return 0;
}
