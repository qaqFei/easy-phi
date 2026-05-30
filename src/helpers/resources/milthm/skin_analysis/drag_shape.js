(t => {
    const r = 0.1381;
    const a = 0.23467;
    t *= a*r / (a-r);
    const v = t <= a ? Math.sqrt(2*r*t - t**2) : ((r*t - r*a - t*a + 2*r*a) / Math.sqrt(2*r*a - a**2));
    return v * (1/r);
})
