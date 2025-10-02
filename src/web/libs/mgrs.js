/* Поддержка сетки MGRS (100 км) для вкладки Pointing.
 * Используется упрощённая адаптация алгоритмов библиотеки proj4js/mgrs (MIT).
 */
(function (global) {
  // Константы UTM/MGRS (адаптированы из proj4js/mgrs)
  const NUM_100K_SETS = 6;
  const SET_ORIGIN_COLUMN_LETTERS = "AJSAJS";
  const SET_ORIGIN_ROW_LETTERS = "AFAFAF";
  const A = 65;
  const I = 73;
  const O = 79;
  const V = 86;
  const Z = 90;
  const ECC_SQUARED = 0.00669438;
  const SCALE_FACTOR = 0.9996;
  const SEMI_MAJOR_AXIS = 6378137;
  const EASTING_OFFSET = 500000;
  const NORTHING_OFFSET = 10000000;
  const UTM_ZONE_WIDTH = 6;
  const HALF_UTM_ZONE_WIDTH = UTM_ZONE_WIDTH / 2;

  // Нормализация ввода — принимаем только обозначения зоны и 100‑км квадрата
  function normalizeInput(value) {
    if (!value) return null;
    const trimmed = String(value).toUpperCase().replace(/[^0-9A-Z]/g, "");
    const match = trimmed.match(/^(\d{1,2})([C-HJ-NP-X])([A-HJ-NP-Z]{2})$/);
    if (!match) return null;
    const zone = parseInt(match[1], 10);
    if (!Number.isFinite(zone) || zone < 1 || zone > 60) return null;
    const e100k = match[3][0];
    const n100k = match[3][1];
    if (e100k === "I" || e100k === "O" || n100k === "I" || n100k === "O") return null;
    return {
      zone,
      band: match[2],
      e100k,
      n100k,
      text: String(zone) + match[2] + e100k + n100k,
    };
  }

  function degToRad(deg) {
    return (deg * (Math.PI / 180));
  }

  function radToDeg(rad) {
    return (180 * (rad / Math.PI));
  }

  function get100kSetForZone(i) {
    let setParm = i % NUM_100K_SETS;
    if (setParm === 0) {
      setParm = NUM_100K_SETS;
    }
    return setParm;
  }

  function getEastingFromChar(e, set) {
    let curCol = SET_ORIGIN_COLUMN_LETTERS.charCodeAt(set - 1);
    let eastingValue = 100000;
    let rewindMarker = false;

    while (curCol !== e.charCodeAt(0)) {
      curCol++;
      if (curCol === I) curCol++;
      if (curCol === O) curCol++;
      if (curCol > Z) {
        if (rewindMarker) {
          throw new Error("Bad character: " + e);
        }
        curCol = A;
        rewindMarker = true;
      }
      eastingValue += 100000;
    }
    return eastingValue;
  }

  function getNorthingFromChar(n, set) {
    if (n > "V") {
      throw new TypeError("MGRSPoint given invalid Northing " + n);
    }
    let curRow = SET_ORIGIN_ROW_LETTERS.charCodeAt(set - 1);
    let northingValue = 0;
    let rewindMarker = false;

    while (curRow !== n.charCodeAt(0)) {
      curRow++;
      if (curRow === I) curRow++;
      if (curRow === O) curRow++;
      if (curRow > V) {
        if (rewindMarker) {
          throw new Error("Bad character: " + n);
        }
        curRow = A;
        rewindMarker = true;
      }
      northingValue += 100000;
    }
    return northingValue;
  }

  function getMinNorthing(zoneLetter) {
    switch (zoneLetter) {
      case "C": return 1100000;
      case "D": return 2000000;
      case "E": return 2800000;
      case "F": return 3700000;
      case "G": return 4600000;
      case "H": return 5500000;
      case "J": return 6400000;
      case "K": return 7300000;
      case "L": return 8200000;
      case "M": return 9100000;
      case "N": return 0;
      case "P": return 800000;
      case "Q": return 1700000;
      case "R": return 2600000;
      case "S": return 3500000;
      case "T": return 4400000;
      case "U": return 5300000;
      case "V": return 6200000;
      case "W": return 7000000;
      case "X": return 7900000;
      default:
        throw new TypeError("Invalid zone letter: " + zoneLetter);
    }
  }

  function decodeMgrs(mgrsString) {
    if (!mgrsString) throw new TypeError("MGRSPoint converting from empty value");
    let value = mgrsString.replace(/ /g, "");
    let i = 0;
    let zoneBuffer = "";
    const length = value.length;

    while (!(/[A-Z]/).test(value.charAt(i))) {
      if (i >= 2) throw new Error("MGRSPoint bad conversion from: " + value);
      zoneBuffer += value.charAt(i);
      i++;
    }

    const zoneNumber = parseInt(zoneBuffer, 10);
    if (i === 0 || i + 3 > length) throw new Error("MGRSPoint bad conversion from " + value);

    const zoneLetter = value.charAt(i++);
    if (zoneLetter <= "A" || zoneLetter === "B" || zoneLetter === "Y" || zoneLetter >= "Z" || zoneLetter === "I" || zoneLetter === "O") {
      throw new Error("MGRSPoint zone letter " + zoneLetter + " not handled: " + value);
    }

    const hunK = value.substring(i, i += 2);
    const set = get100kSetForZone(zoneNumber);
    const east100k = getEastingFromChar(hunK.charAt(0), set);
    let north100k = getNorthingFromChar(hunK.charAt(1), set);
    while (north100k < getMinNorthing(zoneLetter)) {
      north100k += 2000000;
    }

    const remainder = length - i;
    if (remainder % 2 !== 0) {
      throw new Error("MGRSPoint has to have an even number of digits: " + value);
    }

    const sep = remainder / 2;
    let accuracyBonus;
    let sepEasting = 0;
    let sepNorthing = 0;
    if (sep > 0) {
      accuracyBonus = 100000 / Math.pow(10, sep);
      const eastingString = value.substring(i, i + sep);
      const northingString = value.substring(i + sep);
      sepEasting = parseFloat(eastingString) * accuracyBonus;
      sepNorthing = parseFloat(northingString) * accuracyBonus;
    }

    return {
      easting: east100k + sepEasting,
      northing: north100k + sepNorthing,
      zoneLetter,
      zoneNumber,
      accuracy: accuracyBonus,
    };
  }

  function UTMtoLL(utm) {
    const { northing: UTMNorthing, easting: UTMEasting, zoneLetter, zoneNumber } = utm;
    if (zoneNumber < 0 || zoneNumber > 60) return null;

    const a = SEMI_MAJOR_AXIS;
    const e1 = (1 - Math.sqrt(1 - ECC_SQUARED)) / (1 + Math.sqrt(1 - ECC_SQUARED));
    const x = UTMEasting - EASTING_OFFSET;
    let y = UTMNorthing;
    if (zoneLetter < "N") {
      y -= NORTHING_OFFSET;
    }

    const longOrigin = (zoneNumber - 1) * UTM_ZONE_WIDTH - 180 + HALF_UTM_ZONE_WIDTH;
    const eccPrimeSquared = ECC_SQUARED / (1 - ECC_SQUARED);
    const M = y / SCALE_FACTOR;
    const mu = M / (a * (1 - ECC_SQUARED / 4 - 3 * ECC_SQUARED * ECC_SQUARED / 64 - 5 * ECC_SQUARED * ECC_SQUARED * ECC_SQUARED / 256));

    const phi1Rad = mu + (3 * e1 / 2 - 27 * Math.pow(e1, 3) / 32) * Math.sin(2 * mu)
      + (21 * e1 * e1 / 16 - 55 * Math.pow(e1, 4) / 32) * Math.sin(4 * mu)
      + (151 * Math.pow(e1, 3) / 96) * Math.sin(6 * mu);

    const N1 = a / Math.sqrt(1 - ECC_SQUARED * Math.sin(phi1Rad) * Math.sin(phi1Rad));
    const T1 = Math.tan(phi1Rad) * Math.tan(phi1Rad);
    const C1 = eccPrimeSquared * Math.cos(phi1Rad) * Math.cos(phi1Rad);
    const R1 = a * (1 - ECC_SQUARED) / Math.pow(1 - ECC_SQUARED * Math.sin(phi1Rad) * Math.sin(phi1Rad), 1.5);
    const D = x / (N1 * SCALE_FACTOR);

    let lat = phi1Rad - (N1 * Math.tan(phi1Rad) / R1) * (D * D / 2 - (5 + 3 * T1 + 10 * C1 - 4 * C1 * C1 - 9 * eccPrimeSquared) * Math.pow(D, 4) / 24
      + (61 + 90 * T1 + 298 * C1 + 45 * T1 * T1 - 252 * eccPrimeSquared - 3 * C1 * C1) * Math.pow(D, 6) / 720);
    lat = radToDeg(lat);

    let lon = (D - (1 + 2 * T1 + C1) * Math.pow(D, 3) / 6
      + (5 - 2 * C1 + 28 * T1 - 3 * C1 * C1 + 8 * eccPrimeSquared + 24 * T1 * T1) * Math.pow(D, 5) / 120) / Math.cos(phi1Rad);
    lon = longOrigin + radToDeg(lon);

    if (typeof utm.accuracy === "number") {
      const topRight = UTMtoLL({
        northing: utm.northing + utm.accuracy,
        easting: utm.easting + utm.accuracy,
        zoneLetter: utm.zoneLetter,
        zoneNumber: utm.zoneNumber,
      });
      if (!topRight) return null;
      return {
        top: topRight.lat,
        right: topRight.lon,
        bottom: lat,
        left: lon,
      };
    }

    return { lat, lon };
  }

  function toLatLon100k(value) {
    const normalized = normalizeInput(value);
    if (!normalized) return null;
    let utm;
    try {
      utm = decodeMgrs(normalized.text);
    } catch (err) {
      return null;
    }
    if (!utm) return null;
    const point = UTMtoLL(utm);
    if (!point || typeof point.lat !== "number" || typeof point.lon !== "number") return null;
    return {
      lat: point.lat,
      lon: point.lon,
      zone: normalized.zone,
      band: normalized.band,
      square: normalized.e100k + normalized.n100k,
      text: normalized.text,
    };
  }

  global.satMgrs = {
    normalize100k(value) {
      return normalizeInput(value);
    },
    toLatLon100k(value) {
      return toLatLon100k(value);
    },
  };
})(typeof window !== "undefined" ? window : this);
