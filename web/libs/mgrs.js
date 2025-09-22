/* Простейшая поддержка сетки MGRS (100 км) для вкладки Pointing.
 * Конвертирует обозначение зоны/полосы/квадрата в широту и долготу (центр квадрата).
 */
(function (global) {
  const EASTING_SETS = ["ABCDEFGH", "JKLMNPQR", "STUVWXYZ"];
  const NORTHING_LETTERS = "ABCDEFGHJKLMNPQRSTUV";
  const LAT_BANDS = {
    C: [-80, -72],
    D: [-72, -64],
    E: [-64, -56],
    F: [-56, -48],
    G: [-48, -40],
    H: [-40, -32],
    J: [-32, -24],
    K: [-24, -16],
    L: [-16, -8],
    M: [-8, 0],
    N: [0, 8],
    P: [8, 16],
    Q: [16, 24],
    R: [24, 32],
    S: [32, 40],
    T: [40, 48],
    U: [48, 56],
    V: [56, 64],
    W: [64, 72],
    X: [72, 84],
  };
  const WGS84_A = 6378137.0;
  const WGS84_ECC = 0.00669437999014;
  const K0 = 0.9996;
  const RAD2DEG = 180 / Math.PI;

  function normalizeInput(value) {
    if (!value) return null;
    const trimmed = String(value).toUpperCase().replace(/[^0-9A-Z]/g, "");
    const match = trimmed.match(/^(\d{1,2})([C-HJ-NP-X])([A-HJ-NP-Z]{2})$/);
    if (!match) return null;
    const zone = parseInt(match[1], 10);
    if (!Number.isFinite(zone) || zone < 1 || zone > 60) return null;
    const band = match[2];
    const e100k = match[3][0];
    const n100k = match[3][1];
    if (e100k === "I" || e100k === "O" || n100k === "I" || n100k === "O") return null;
    return {
      zone,
      band,
      e100k,
      n100k,
      text: String(zone) + band + e100k + n100k,
    };
  }

  function utmToLatLon(zone, easting, northing, hemisphere) {
    const eccPrimeSquared = WGS84_ECC / (1 - WGS84_ECC);
    const e1 = (1 - Math.sqrt(1 - WGS84_ECC)) / (1 + Math.sqrt(1 - WGS84_ECC));
    const x = easting - 500000.0;
    let y = northing;
    if (hemisphere === "S") {
      y -= 10000000.0;
    }
    const M = y / K0;
    const mu = M /
      (WGS84_A * (1 - WGS84_ECC / 4 - (3 * WGS84_ECC * WGS84_ECC) / 64 - (5 * Math.pow(WGS84_ECC, 3)) / 256));
    const phi1Rad =
      mu + (3 * e1 / 2 - 27 * Math.pow(e1, 3) / 32) * Math.sin(2 * mu) +
      (21 * e1 * e1 / 16 - 55 * Math.pow(e1, 4) / 32) * Math.sin(4 * mu) +
      (151 * Math.pow(e1, 3) / 96) * Math.sin(6 * mu) +
      (1097 * Math.pow(e1, 4) / 512) * Math.sin(8 * mu);
    const sinPhi1 = Math.sin(phi1Rad);
    const cosPhi1 = Math.cos(phi1Rad);
    const tanPhi1 = Math.tan(phi1Rad);
    const N1 = WGS84_A / Math.sqrt(1 - WGS84_ECC * sinPhi1 * sinPhi1);
    const R1 = WGS84_A * (1 - WGS84_ECC) / Math.pow(1 - WGS84_ECC * sinPhi1 * sinPhi1, 1.5);
    const T1 = tanPhi1 * tanPhi1;
    const C1 = eccPrimeSquared * cosPhi1 * cosPhi1;
    const D = x / (N1 * K0);

    const lat = phi1Rad -
      (N1 * tanPhi1 / R1) *
        (D * D / 2 - (5 + 3 * T1 + 10 * C1 - 4 * C1 * C1 - 9 * eccPrimeSquared) * Math.pow(D, 4) / 24 +
          (61 + 90 * T1 + 298 * C1 + 45 * T1 * T1 - 252 * eccPrimeSquared - 3 * C1 * C1) * Math.pow(D, 6) / 720);
    const lon =
      (D - (1 + 2 * T1 + C1) * Math.pow(D, 3) / 6 +
        (5 - 2 * C1 + 28 * T1 - 3 * C1 * C1 + 8 * eccPrimeSquared + 24 * T1 * T1) * Math.pow(D, 5) / 120) /
      cosPhi1;
    const lonOrigin = (zone - 1) * 6 - 180 + 3;
    return {
      lat: lat * RAD2DEG,
      lon: lon * RAD2DEG + lonOrigin,
    };
  }

  function solveLatLon(data) {
    const bandRange = LAT_BANDS[data.band];
    if (!bandRange) return null;
    const eastingLetters = EASTING_SETS[(data.zone - 1) % 3];
    const colIndex = eastingLetters.indexOf(data.e100k);
    if (colIndex < 0) return null;
    const easting = 100000 + colIndex * 100000 + 50000;

    const northingOffset = data.zone % 2 === 0 ? 5 : 0;
    const rotated = NORTHING_LETTERS.slice(northingOffset) + NORTHING_LETTERS.slice(0, northingOffset);
    const rowIndex = rotated.indexOf(data.n100k);
    if (rowIndex < 0) return null;
    const baseNorthing = rowIndex * 100000 + 50000;

    const hemisphere = data.band >= "N" ? "N" : "S";
    const cycle = 2000000;
    let candidate = baseNorthing + (hemisphere === "S" ? 10000000 : 0);
    let result = utmToLatLon(data.zone, easting, candidate, hemisphere);
    let attempts = 0;
    while ((result.lat < bandRange[0] || result.lat >= bandRange[1]) && attempts < 10) {
      if (result.lat < bandRange[0]) {
        candidate += cycle;
      } else {
        candidate -= cycle;
        if (candidate <= 0 && hemisphere !== "S") break;
      }
      result = utmToLatLon(data.zone, easting, candidate, hemisphere);
      attempts += 1;
    }
    if (result.lat < bandRange[0] || result.lat >= bandRange[1]) return null;
    return {
      lat: result.lat,
      lon: result.lon,
      zone: data.zone,
      band: data.band,
      square: data.e100k + data.n100k,
    };
  }

  global.satMgrs = {
    normalize100k(value) {
      return normalizeInput(value);
    },
    toLatLon100k(value) {
      const normalized = normalizeInput(value);
      if (!normalized) return null;
      const solved = solveLatLon(normalized);
      if (!solved) return null;
      solved.text = normalized.text;
      return solved;
    },
  };
})(typeof window !== "undefined" ? window : this);
