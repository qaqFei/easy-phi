CanvasRenderingContext2D.prototype.__c2dextopt_drawImage = CanvasRenderingContext2D.prototype.drawImage;
CanvasRenderingContext2D.prototype.drawImage = function(...args) {
    if (!args[0] || !args[0].width || !args[0].height) return;
    return this.__c2dextopt_drawImage(...args);
}

CanvasRenderingContext2D.prototype.drawRotateImage = function(im, x, y, width, height, deg, alpha) { // draw at the position center
    this.save();
    this.globalAlpha *= alpha;
    if (!!deg) {
        this.translate(x, y);
        this.rotate(deg * Math.PI / 180);
        this.drawImage(im, -width / 2, -height / 2, width, height);
    } else {
        this.drawImage(im, x - width / 2, y - height / 2, width, height);
    }
    this.restore();
};

CanvasRenderingContext2D.prototype.drawAnchorESRotateImage = function(im, x, y, width, height, deg, alpha) {
    this.save();
    this.globalAlpha *= alpha;
    if (!!deg) {
        this.translate(x, y);
        this.rotate(deg * Math.PI / 180);
        this.drawImage(im, -width / 2, -height, width, height);
    } else {
        this.drawImage(im, x - width / 2, y - height, width, height);
    }
    this.restore();
};

CanvasRenderingContext2D.prototype.drawAnchorWCenterRotateImage = function(im, x, y, width, height, deg, alpha, mdx) {
    this.save();
    this.globalAlpha *= alpha;
    if (!!deg) {
        this.translate(x, y);
        this.rotate(deg * Math.PI / 180);
        this.drawImage(im, mdx, -height / 2, width, height);
    } else {
        this.drawImage(im, mdx, y - height / 2, width, height);
    }
    this.restore();
};

CanvasRenderingContext2D.prototype.drawScaleImage = function(im, x, y, width, height, xs, ys) {
    x += width / 2; y += height / 2;
    this.save();
    this.translate(x, y);
    this.scale(xs, ys);
    this.drawImage(im, -width / 2, -height / 2, width, height);
    this.restore();
};

CanvasRenderingContext2D.prototype.drawRPEMultipleRotateText = function(text, x, y, deg, fontsize, color, xs, ys) {
    this.save();
    this.translate(x, y);
    this.rotate(deg * Math.PI / 180);
    this.scale(xs, ys);
    this.fillStyle = color;
    this.textAlign = "center";
    this.textBaseline = "middle";
    this.font = `${fontsize}px pgrFont`;

    if (text.includes("\n") && RPEVersion >= 150) {
        let texts = text.split("\n");
        let x = 0.0; let y = 0.0;
        for (let currtext of texts) {
            if (currtext) this.fillText(currtext, x, y);
            let measure = this.measureText(currtext);
            y += (measure.actualBoundingBoxDescent + measure.actualBoundingBoxAscent) * 1.25;
        }
    }
    else {
        this.fillText(text, 0, 0);
    }

    this.restore();
};

CanvasRenderingContext2D.prototype.drawRotateText = function(text, x, y, deg, fontsize, color, xscale, yscale) {
    this.save();
    this.translate(x, y);
    this.rotate(deg * Math.PI / 180);
    this.scale(xscale, yscale);
    this.fillStyle = color;
    this.textAlign = "center";
    this.textBaseline = "middle";
    this.font = `${fontsize}px pgrFont`;
    this.fillText(text, 0, 0);
    this.restore();
};

CanvasRenderingContext2D.prototype.drawAlphaImage = function(im, x, y, width, height, alpha) {
    this.save()
    this.globalAlpha *= alpha;
    this.drawImage(im, x, y, width, height);
    this.restore();
};

CanvasRenderingContext2D.prototype.drawAlphaCenterImage = function(im, x, y, width, height, alpha) {
    this.save()
    this.globalAlpha *= alpha;
    this.drawImage(im, x - width / 2, y - height / 2, width, height);
    this.restore();
};

CanvasRenderingContext2D.prototype.drawTextEx = function(text, x, y, font, color, align, baseline) {
    this.save();
    this.fillStyle = color;
    this.textAlign = align;
    this.textBaseline = baseline;
    this.font = font;
    this.fillText(text, x, y);
    this.restore();
};

CanvasRenderingContext2D.prototype.fillRectEx = function(x, y, w, h, color) {
    this.save();
    this.fillStyle = color;
    this.fillRect(x, y, w, h);
    this.restore();
};

CanvasRenderingContext2D.prototype.fillRectExConvert2LeftCenter = function(x, y, w, h, color) {
    y += h / 2;
    this.save();
    this.fillStyle = color;
    this.beginPath();
    this.moveTo(x, y - h / 2);
    this.lineTo(x + w, y - h / 2);
    this.lineTo(x + w, y + h / 2);
    this.lineTo(x, y + h / 2);
    this.closePath();
    this.fill();
    this.restore();
};

CanvasRenderingContext2D.prototype.fillRectExByRect = function(x0, y0, x1, y1, color) {
    return this.fillRectEx(x0, y0, x1 - x0, y1 - y0, color);
};

CanvasRenderingContext2D.prototype.strokeRectEx = function(x, y, w, h, color, width) {
    this.save();
    this.strokeStyle = color;
    this.lineWidth = width;
    this.strokeRect(x, y, w, h);
    this.restore();
};

CanvasRenderingContext2D.prototype.strokeRectEx = function(x, y, w, h, color, width) {
    this.save();
    this.strokeStyle = color;
    this.lineWidth = width;
    this.strokeRect(x, y, w, h);
    this.restore();
};

CanvasRenderingContext2D.prototype.addRoundRectData = function(x, y, w, h, r) {
    if (this._roundDatas == undefined) this._roundDatas = [];
    this._roundDatas.push({ x: x, y: y, w: w, h: h, r: r });
};

CanvasRenderingContext2D.prototype.drawRoundDatas = function(color) {
    if (this._roundDatas) {
        this.roundRectsEx(color, this._roundDatas);
        this._roundDatas = undefined;
    }
};

CanvasRenderingContext2D.prototype.roundRectsEx = function(color, datas) {
    this.save();
    this.fillStyle = color;
    this.beginPath();
    for (let i of datas) {
        this.roundRect(i.x, i.y, i.w, i.h, i.r);
    }
    this.fill();
    this.restore();
};

CanvasRenderingContext2D.prototype.drawLineEx = function(x1, y1, x2, y2, width, color) {
    this.save();
    this.strokeStyle = color;
    this.lineWidth = width;
    this.beginPath();
    this.moveTo(x1, y1);
    this.lineTo(x2, y2);
    this.stroke();
    this.restore();
};

CanvasRenderingContext2D.prototype.diagonalRectangle = function(x0, y0, x1, y1, power) {
    // x0 = Math.floor(x0);
    // y0 = Math.floor(y0);
    // x1 = Math.floor(x1);
    // y1 = Math.floor(y1);
    this.moveTo(x0 + (x1 - x0) * power, y0);
    this.lineTo(x1, y0);
    this.lineTo(x1 - (x1 - x0) * power, y1);
    this.lineTo(x0, y1);
    this.lineTo(x0 + (x1 - x0) * power, y0);
};

CanvasRenderingContext2D.prototype.clipDiagonalRectangle = function(x0, y0, x1, y1, power) {
    this.beginPath();
    this.diagonalRectangle(x0, y0, x1, y1, power);
    this.clip();
};

CanvasRenderingContext2D.prototype.clipRect = function(x0, y0, x1, y1) {
    this.beginPath();
    this.rect(x0, y0, x1 - x0, y1 - y0);
    this.clip();
};

CanvasRenderingContext2D.prototype.drawClipXText = function(text, x, y, align, baseline, color, font, clipx0, clipx1) {
    this.save();
    this.clipRect(clipx0, 0, clipx1, this.canvas.height);
    this.fillStyle = color;
    this.textAlign = align;
    this.textBaseline = baseline;
    this.font = font;
    this.fillText(text, x, y);
    this.restore();
};

CanvasRenderingContext2D.prototype.drawDiagonalRectangle = function(x0, y0, x1, y1, power, color) {
    this.save();
    this.fillStyle = color;
    this.beginPath();
    this.diagonalRectangle(x0, y0, x1, y1, power);
    this.fill();
    this.restore();
};

CanvasRenderingContext2D.prototype.drawDiagonalRectangleShadow = function(x0, y0, x1, y1, power, color, shadowColor, shadowBlur) {
    this.save();
    this.shadowColor = shadowColor;
    this.shadowBlur = shadowBlur;
    this.fillStyle = color;
    this.beginPath();
    this.diagonalRectangle(x0, y0, x1, y1, power);
    this.fill();
    this.restore();
};

CanvasRenderingContext2D.prototype.drawDiagonalDialogRectangleText = function(x0, y0, x1, y1, power, text1, text2, color, font) {
    this.save();
    this.fillStyle = color;
    this.font = font;
    this.textBaseline = "middle";
    this.textAlign = "left";
    this.fillText(text1, x0 + (x1 - x0) * power * 3.0, y0 + (y1 - y0) * 0.5);
    this.textAlign = "right";
    this.fillText(text2, x1 - (x1 - x0) * power * 2.0, y0 + (y1 - y0) * 0.5);
    this.restore();
};

CanvasRenderingContext2D.prototype.drawDiagonalRectangleClipImage = function(x0, y0, x1, y1, im, imx, imy, imw, imh, power, alpha) {
    if (alpha == 0.0) return;
    this.save();
    this.globalAlpha *= alpha;
    this.beginPath();
    this.diagonalRectangle(x0, y0, x1, y1, power);
    this.clip();
    this.drawImage(im, x0 + imx, y0 + imy, imw, imh);
    this.restore();
};

CanvasRenderingContext2D.prototype.drawDiagonalRectangleCoverClipImage = function(x0, y0, x1, y1, im, power, alpha) {
    if (alpha == 0.0) return;
    this.save();
    this.globalAlpha *= alpha;
    this.beginPath();
    this.diagonalRectangle(x0, y0, x1, y1, power);
    this.clip();
    this.drawCoverFullScreenImage(im, x1 - x0, y1 - y0, x0, y0);
    this.restore();
};

CanvasRenderingContext2D.prototype.drawGrd = function(grdpos, steps, x0, y0, x1, y1) {
    const grd = this.createLinearGradient(...grdpos);
    for (const step of steps) {
        grd.addColorStop(...step);
    }

    // x0 = Math.floor(x0);
    // y0 = Math.floor(y0);
    // x1 = Math.floor(x1);
    // y1 = Math.floor(y1);
    this.save();
    this.fillStyle = grd;
    this.fillRect(x0, y0, x1 - x0, y1 - y0);
    this.restore();
};

CanvasRenderingContext2D.prototype.drawDiagonalGrd = function(x0, y0, x1, y1, power, steps, grdpos) {
    this.save();
    this.beginPath();
    this.diagonalRectangle(x0, y0, x1, y1, power);
    this.clip();
    this.drawGrd(grdpos, steps, x0, y0, x1, y1);
    this.restore();
};

CanvasRenderingContext2D.prototype.drawDiagonalRectangleClipImageOnlyHeight = function(x0, y0, x1, y1, im, imh, power, alpha) {
    const [irw, irh] = [
        im.__drawImage__ ? im.__drawImage__().width : im.width,
        im.__drawImage__ ? im.__drawImage__().height : im.height
    ]

    let imw = imh * irw / irh;

    if (imw < x1 - x0) {
        imw = x1 - x0;
        imh = imw * irh / irw;
    }

    if (isNaN(imw) || isNaN(imh)){
        imw = this.canvas.width;
        imh = this.canvas.height;
    }

    let imx = (x1 - x0) / 2 - imw / 2;
    let imy = (y1 - y0) / 2 - imh / 2;
    return this.drawDiagonalRectangleClipImage(x0, y0, x1, y1, im, imx, imy, imw, imh, power, alpha);
};

CanvasRenderingContext2D.prototype.drawDiagonalRectangleClipImageOnlyWidth = function(x0, y0, x1, y1, im, imw, power, alpha) {
    const [irw, irh] = [
        im.__drawImage__ ? im.__drawImage__().width : im.width,
        im.__drawImage__ ? im.__drawImage__().height : im.height
    ]

    let imh = imw / irw * irh;

    if (imh < y1 - y0) {
        imh = y1 - y0;
        imw = imh / irh * irw;
    }

    if (isNaN(imw) || isNaN(imh)){
        imw = this.canvas.width;
        imh = this.canvas.height;
    }

    let imx = (x1 - x0) / 2 - imw / 2;
    let imy = (y1 - y0) / 2 - imh / 2;
    return this.drawDiagonalRectangleClipImage(x0, y0, x1, y1, im, imx, imy, imw, imh, power, alpha);
};

CanvasRenderingContext2D.prototype.drawRotateText2 = function(text, x, y, deg, color, font, align, baseline) {
    this.save();
    this.translate(x, y);
    this.rotate(deg * Math.PI / 180);
    this.fillStyle = color;
    this.textAlign = align;
    this.textBaseline = baseline;
    this.font = font;
    this.fillText(text, 0, 0);
    this.restore();
};

CanvasRenderingContext2D.prototype.drawTriangleFrame = function(x0, y0, x1, y1, x2, y2, color, width) {
    this.save();
    this.strokeStyle = color;
    this.lineWidth = width;
    this.beginPath();
    this.moveTo(x0, y0);
    this.lineTo(x1, y1);
    this.lineTo(x2, y2);
    this.closePath();
    this.stroke();
    this.restore();
};

CanvasRenderingContext2D.prototype.drawRectMultilineText = function(x0, y0, x1, y1, text, color, font, fontsize, lineOffsetScale) {
    this.save();

    this.font = font;
    this.fillStyle = color;
    this.textBaseline = "top";
    this.textAlign = "left";
    let texts = splitText(x0, y0, x1, y1, text, this);

    this.rect(x0, y0, x1 - x0, y1 - y0);
    let dy = 0.0;

    for (let i = 0; i < texts.length; i++) {
        this.fillText(texts[i], x0, y0 + dy);
        dy += fontsize * lineOffsetScale;
        if (dy >= (y1 - y0)) break;
    }

    this.restore();
    return texts.length * fontsize * lineOffsetScale;
};

CanvasRenderingContext2D.prototype.drawRectMultilineTextDiagonal = function(x0, y0, x1, y1, text, color, font, fontsize, lineDiagonal, lineOffsetScale) {
    this.save();

    this.font = font;
    this.fillStyle = color;
    this.textBaseline = "top";
    this.textAlign = "left";
    let texts = splitText(x0, y0, x1, y1, text, this);

    this.rect(x0, y0, x1 - x0, y1 - y0);
    let dx = 0.0;
    let dy = 0.0;

    for (let i = 0; i < texts.length; i++) {
        if (texts[i]) {
            this.fillText(texts[i], x0 + dx, y0 + dy);
            dy += fontsize * lineOffsetScale;
            dx += lineDiagonal;
        } else {
            dx += lineDiagonal * 0.5;
            dy += fontsize * lineOffsetScale * 0.5;
        }
        if (dy >= (y1 - y0)) break;
    }

    this.restore();
    return texts.length * fontsize * lineOffsetScale;
};

CanvasRenderingContext2D.prototype.drawRectMultilineTextCenter = function(x0, y0, x1, y1, text, color, font, fontsize, lineOffsetScale) {
    this.save();

    this.font = font;
    this.fillStyle = color;
    this.textBaseline = "top";
    this.textAlign = "center";
    let texts = splitText(x0, y0, x1, y1, text, this);

    this.rect(x0, y0, x1 - x0, y1 - y0);
    let dy = 0.0;

    for (let i = 0; i < texts.length; i++) {
        this.fillText(texts[i], x0 + (x1 - x0) / 2, y0 + dy);
        dy += fontsize * lineOffsetScale;
        if (dy >= (y1 - y0)) break;
    }

    this.restore();
    return texts.length * fontsize * lineOffsetScale;
};

CanvasRenderingContext2D.prototype.drawUIItems = function(datas) {
    for (let i of datas) {
        if (i == null) continue;

        if (i.type == "text") {
            this.save();
            this.font = `${i.weight ? `${i.weight} ` : ""}${i.fontsize}px pgrFont`;
            this.textBaseline = i.textBaseline;
            this.textAlign = i.textAlign;
            this.fillStyle = i.color;
            this.translate(i.x + i.dx, i.y + i.dy);
            if (i.sx != 1.0 || i.sy != 1.0) this.scale(i.sx, i.sy);
            if (i.rotate != 0.0) this.rotate(i.rotate * Math.PI / 180);
            this.fillText(i.text, 0, 0);
            this.restore();
        }
        else if (i.type == "image") {
            this.save();
            const img = eval(i.image);
            const [r, g, b, a] = i.color;
            this.translate(i.x + i.dx, i.y + i.dy);
            if (i.rotate != 0.0) this.rotate(i.rotate * Math.PI / 180);
            if (a != 1.0) this.globalAlpha = a;
            if (r != 255 || g != 255 || b != 255) {
                setColorMatrix(r, g, b);
                this.filter = "url(#textureLineColorFilter)";
            }
            this.drawImage(img, 0, 0, i.width, i.height);
            this.restore();
        }
        else if (i.type == "call") {
            this[i.name](...i.args);
        }
        else if (i.type == "pbar") {
            const { w, pw, process } = i;

            this.save();
            // if (i.dx != 0.0 || i.dy != 0.0) this.translate(i.dx, i.dy);
            // if (i.rotate != 0.0) this.rotate(i.rotate * Math.PI / 180);
            // if (i.scale != 0.0) this.scale(i.sx, i.sy);

            const [r, g, b, a] = i.color.split("(")[1].split(")")[0].split(", ");
            this.fillRectExConvert2LeftCenter(0, 0, w * process, pw, `rgba(${145 * r / 255}, ${145 * g / 255}, ${145 * b / 255}, ${0.85 * a})`);
            this.fillRectExConvert2LeftCenter(w * process - w * 0.00175, 0, w * 0.00175, pw, `rgba(${r}, ${g}, ${b}, ${0.9 * a})`);
            this.restore();
        }
    }
};

CanvasRenderingContext2D.prototype.drawCoverFullScreenImage = function (img, w, h, x = 0, y = 0, draw = true) {
    let [imw, imh] = [img.width, img.height];
    const ratio = w / h;
    const imratio = imw / imh;

    if (imratio > ratio) {
        imh = h;
        imw = imh * imratio;
    } else {
        imw = w;
        imh = imw / imratio;
    }

    const [imx, imy] = [(w - imw) / 2, (h - imh) / 2];

    this.save();
    this.beginPath();
    this.rect(x, y, w, h);
    this.clip();

    if (draw) this.drawImage(img, x + imx, y + imy, imw, imh);

    this.restore();
    return [imx, imy, imw, imh];
};

CanvasRenderingContext2D.prototype.outOfTransformDrawCoverFullscreenChartBackgroundImage = function (img) {
    this.save();
    this.resetTransform();
    const [imx, imy, imw, imh] = this.drawCoverFullScreenImage(img, this.canvas.width, this.canvas.height);
    this.fillRectEx(imx, imy, imw, imh, `rgba(0.1, 0.1, 0.1, 0.7)`);
    this.restore();
};

CanvasRenderingContext2D.prototype.drawMirrorImage = function (img, x, y, width, height, alpha) {
    this.save();
    this.translate(x + width, y);
    this.scale(-1, 1);
    this.globalAlpha = alpha;
    this.drawImage(img, 0, 0, width, height);
    this.restore();
};

CanvasRenderingContext2D.prototype.drawMirrorRotateImage = function (img, x, y, width, height, rotate, alpha) {
    this.save();
    this.translate(x + width, y);
    this.rotate(rotate * Math.PI / 180);
    this.scale(-1, 1);
    this.globalAlpha = alpha;
    this.drawImage(img, width / 2, -height / 2, width, height);
    this.restore();
};

CanvasRenderingContext2D.prototype.getTextSizeWithoutFont = function (text) {
    const measure = this.measureText(text);
    return [measure.width, measure.actualBoundingBoxAscent + measure.actualBoundingBoxDescent];
}

CanvasRenderingContext2D.prototype.getTextSize = function (text, font) {
    this.save();
    this.font = font;
    const res = this.getTextSizeWithoutFont(text);
    this.restore();
    return res;
};

CanvasRenderingContext2D.prototype.setShadow = function (color, blur, dx = 0, dy = 0) {
    this.save();
    this.shadowColor = color;
    this.shadowBlur = blur;
    this.shadowOffsetX = dx;
    this.shadowOffsetY = dy;
};

CanvasRenderingContext2D.prototype.mirror = function () {
    this.save();
    this.scale(-1, 1);
    this.translate(-this.canvas.width, 0);
    this.drawImage(this.canvas, 0, 0);
    this.restore();
};

CanvasRenderingContext2D.prototype.drawLeftBottomSkewText = function (text, x, y, font, color, dpower) {
    this.save();
    this.fillStyle = color;
    this.font = font;
    this.textAlign = "left";
    this.textBaseline = "bottom";
    const [text_w, text_h] = this.getTextSize(text, font);
    let skew_ratio = text_w * dpower / text_h;
    this.transform(
        1.0, 0.0, -skew_ratio,
        1.0, x, y
    );
    this.fillText(text, 0, 0);
    this.restore();
};

CanvasRenderingContext2D.prototype.clear = function() {
    this.save();
    this.setTransform(1, 0, 0, 1, 0, 0);
    this.clearRect(0, 0, this.canvas.width, this.canvas.height);
    this.restore();
};

(() => {
    let tempcv = document.createElement("canvas");
    let tempctx = tempcv.getContext("2d");

    CanvasRenderingContext2D.prototype.createTempClip = function (im, atlas, width, head_length, body_length, tail_length) {
        width = Math.floor(width);
        head_length = Math.floor(head_length);
        body_length = Math.floor(body_length);
        tail_length = Math.floor(tail_length);
        
        tempcv.width = head_length + body_length + tail_length;
        tempcv.height = width;

        const head_scale = head_length / atlas[0];
        const body_scale = body_length / (im.width - atlas[0] - atlas[1]);
        const tail_scale = tail_length / atlas[1];

        tempctx.clear();
        tempctx.drawImage(im, 0, 0, im.width * head_scale, tempcv.height);
        tempctx.clearRect(head_length, 0, tempcv.width, tempcv.height);
        tempctx.translate(head_length, 0);

        tempctx.save();
        tempctx.beginPath();
        tempctx.rect(0, 0, tempcv.width, tempcv.height);
        tempctx.clip();
        tempctx.drawImage(im, -atlas[0] * body_scale, 0, im.width * body_scale, tempcv.height);
        tempctx.restore();
        tempctx.clearRect(body_length, 0, tempcv.width, tempcv.height);
        tempctx.translate(body_length, 0);

        tempctx.save();
        tempctx.beginPath();
        tempctx.rect(0, 0, tempcv.width, tempcv.height);
        tempctx.clip();
        tempctx.drawImage(im, -atlas[0] * tail_scale - (im.width - atlas[0] - atlas[1]) * tail_scale, 0, im.width * tail_scale, tempcv.height);
        tempctx.restore();

        return tempcv;
    };
})();

(() => {
    let tempcv = document.createElement("canvas");
    let tempctx = tempcv.getContext("2d");

    CanvasRenderingContext2D.prototype.drawMaskImage = function (img, mask, x, y, w, h, w2, h2) {
        tempcv.width = w;
        tempcv.height = h;
        tempctx.clear();

        tempctx.drawImage(mask, 0, 0, w, h);
        tempctx.globalCompositeOperation = "source-in";
        tempctx.drawImage(img, (w - w2) / 2, (h - h2) / 2, w2, h2);

        this.drawImage(tempcv, x, y);
    };
})();

CanvasRenderingContext2D.prototype.drawRotateEllipse = function (x, y, radiusX, radiusY, deg, fillStyle) {
    this.save();
    this.beginPath();
    this.fillStyle = fillStyle;
    this.ellipse(x, y, radiusX, radiusY, deg * Math.PI / 180, 0, Math.PI * 2);
    this.fill();
    this.restore();
};

CanvasRenderingContext2D.prototype.drawImage4PTransformed = function(img, ltx, lty, rtx, rty, lbx, lby, rbx, rby) {
    const computeTransform = (src, dest) => {
        const [x0, y0, x1, y1, x2, y2] = src;
        const [u0, v0, u1, v1, u2, v2] = dest;
        
        const denom = (x0 - x2) * (y1 - y2) - (x1 - x2) * (y0 - y2);
        if (Math.abs(denom) < 1e-6) return [1, 0, 0, 1, 0, 0];
        
        const a = ((u0 - u2) * (y1 - y2) - (u1 - u2) * (y0 - y2)) / denom;
        const b = ((u1 - u2) * (x0 - x2) - (u0 - u2) * (x1 - x2)) / denom;
        const c = u2 - a * x2 - b * y2;
        
        const d = ((v0 - v2) * (y1 - y2) - (v1 - v2) * (y0 - y2)) / denom;
        const e = ((v1 - v2) * (x0 - x2) - (v0 - v2) * (x1 - x2)) / denom;
        const f = v2 - d * x2 - e * y2;
        
        return [a, d, b, e, c, f];
    };

    const w = img.width;
    const h = img.height;

    this.save();
    this.beginPath();
    this.moveTo(ltx, lty);
    this.lineTo(rtx, rty);
    this.lineTo(lbx, lby);
    this.closePath();
    this.clip();
            
    const denom1 = w * h;
    if (Math.abs(denom1) > 1e-6) {
        const matrix1 = computeTransform(
            [0, 0, w, 0, 0, h], 
            [ltx, lty, rtx, rty, lbx, lby]
        );
        
        this.transform(...matrix1);
        this.drawImage(img, 0, 0);
    }
    this.restore();

    this.save();
    this.beginPath();
    this.moveTo(lbx, lby);
    this.lineTo(rtx, rty);
    this.lineTo(rbx, rby);
    this.closePath();
    this.clip();
            
    const denom2 = w * h;

    if (Math.abs(denom2) > 1e-6) {
        const matrix2 = computeTransform(
            [0, h, w, 0, w, h], 
            [lbx, lby, rtx, rty, rbx, rby]
        );
        
        this.transform(...matrix2);
        this.drawImage(img, 0, 0);
    }

    this.restore();
};

CanvasRenderingContext2D.prototype.getTransformedPoint = function (x, y) {
    const m = this.getTransform();
    return [m.a * x + m.c * y + m.e, m.b * x + m.d * y + m.f];
};

CanvasRenderingContext2D.prototype.roundRectEx = function (x, y, w, h, r, color) {
    this.save();
    this.beginPath();
    this.roundRect(x, y, w, h, r);
    this.fillStyle = color;
    this.fill();
    this.restore();
};

class _StringReader {
    constructor(str) {
        this.chars = Array.from(str);
        this.pos   = 0;
    }
  
    read(n) {
        if (this.pos >= this.chars.length) return undefined;
        const ret = this.chars.slice(this.pos, this.pos + n).join("");
        this.pos += n;
        return ret;
    }
  
    readUntil(c) {
        const idx = this.chars.indexOf(c, this.pos);
        if (idx === -1) return undefined;
        const ret = this.chars.slice(this.pos, idx).join("");
        this.pos = idx + 1;
        return ret;
    }
}

class UnityRichText {
    constructor(inp) {
        if (typeof inp === "string") this.parse(inp);
        else {
            this.raw = "";
            this.parsed = inp.slice();
            this._spwan_merged();
        }
    }

    parse(raw) {
        this.raw = raw;

        this.parsed = [];
        const reader = new _StringReader(raw);
        let next_char;
        let tag_name;
        let tags = [];

        let remove_tag = tag => {
            const new_arr = [];
            for (let i = 0; i < tags.length; i++) {
                if (tags[i].name !== tag) new_arr.push(tags[i]);
            }
            tags = new_arr;
        };

        while ((next_char = reader.read(1)) !== void 0) {
            if (next_char === "<") {
                tag_name = reader.readUntil(">");
                if (tag_name === void 0) {
                    reader.pos -= tag_name.length;
                } else {
                    if (tag_name.startsWith("/")) {
                        remove_tag(tag_name.substring(1));
                    } else {
                        let tag = {
                            name: tag_name,
                            attributes: {}
                        };

                        if (tag_name.startsWith("color=")) {
                            tag.attributes.color = tag_name.substring("color=".length);
                        }

                        tags.push(tag);
                    }

                    continue;
                }
            }

            this.parsed.push({
                char: next_char,
                tags: tags.slice()
            });
        }

        this._spwan_merged();
    }

    _spwan_merged() {
        this.merged = this.parsed.map(item => item.char).join("");
    }
}

UnityRichText.fs_mul = 1.05;

CanvasRenderingContext2D.prototype.fillUnityRichText = function (richtext, x, y, fontsize, fontfam, color, align, baseline, maxwidth, spacing = 1.0) {
    const fs_mul = UnityRichText.fs_mul * spacing;

    this.save();
    this.translate(x, y);
    this.textAlign = "center";
    this.textBaseline = baseline;
    this.font = `${fontsize}px ${fontfam}`;

    let width = this.measureText(richtext.merged).width * fs_mul;

    while (maxwidth !== void 0 && width > maxwidth) {
        fontsize -= 1.0;
        this.font = `${fontsize}px ${fontfam}`;
        width = this.measureText(richtext.merged).width * fs_mul;
    }

    let font = this.font;

    ctx.translate(-width / 2, 0);

    if (align == "left") ctx.translate(width / 2, 0);
    else if (align == "right") ctx.translate(-width / 2, 0);

    let current_x = 0;
    for (const char of richtext.parsed) {
        const item_width = this.measureText(char.char).width * fs_mul;
        this.font = font;
        this.fillStyle = color;

        for (const tag of char.tags) {
            if (tag.name == "color") {
                this.fillStyle = tag.attributes.color;
            } else if (tag.name == "b") {
                this.font += "Bold";
            }
        }

        this.fillText(char.char, current_x + item_width / 2, 0);

        current_x += item_width;
    }

    this.restore();
};

CanvasRenderingContext2D.prototype.fillUnityRichTextMultiline = function (rawtext, x, y, fontsize, fontfam, color, align, yalign, maxwidth, draw = true) {
    this.save();
    this.font = `${fontsize}px ${fontfam}`;

    const raw_lines = rawtext.split("\n");
    const raw_richs = raw_lines.map(line => new UnityRichText(line));

    const new_richs = [];
    for (const raw_richitem of raw_richs) {
        if (!raw_richitem.parsed.length) {
            new_richs.push(new UnityRichText(""));
            continue;
        }

        while (raw_richitem.parsed.length) {
            const line = [];
            let total_string = "";

            for (const char of raw_richitem.parsed) {
                total_string += char.char;
                if (this.measureText(total_string).width * UnityRichText.fs_mul > maxwidth) break;
                line.push(char);
            }

            raw_richitem.parsed = raw_richitem.parsed.slice(line.length);
            new_richs.push(new UnityRichText(line));
        }
    }

    const height = fontsize * UnityRichText.fs_mul * new_richs.length;

    if (draw) {
        this.translate(x, y);

        if (yalign == "top") {
            this.translate(0, 0);
        } else if (yalign == "middle") {
            this.translate(0, -height / 2);
        } else if (yalign == "bottom") {
            this.translate(0, -height);
        }
    }

    let curry = 0.0;
    for (const rich of new_richs) {
        if (draw) {
            this.fillUnityRichText(rich, 0, curry, fontsize, fontfam, color, align, "top");
        }
        curry += fontsize * UnityRichText.fs_mul;
    }

    this.restore();

    return curry;
};

(() => {
    const cache = new WeakMap();

    CanvasRenderingContext2D.prototype.getBlurImage = function (img, blur) {
        const cache_blurs = cache.get(img);
        if (cache_blurs && cache_blurs.has(blur)) return cache_blurs.get(blur);

        const temp_cv = document.createElement("canvas");
        const temp_ctx = temp_cv.getContext("2d");
        temp_cv.width = img.width;
        temp_cv.height = img.height;
        temp_ctx.clear();

        temp_ctx.filter = `blur(${blur * (img.width + img.height)}px)`;
        temp_ctx.drawImage(img, 0, 0);

        if (!cache.has(img)) cache.set(img, new Map());

        cache.get(img).set(blur, temp_cv);

        return temp_cv;
    };
})();

CanvasRenderingContext2D.prototype.drawMilthmStartScene = function (
    bg, bird, charater,
    w, h, dx, dy,
    move3dk, charater_foot_point,
    t
) {
    const maxmove = (w + h) * move3dk * 1.5;
    const scale = Math.max(
        (w + maxmove) / w,
        (h + maxmove) / h
    );

    dx *= move3dk; dy *= move3dk;

    this.save();
    this.translate(w / 2, h / 2);
    this.scale(scale, scale);
    this.translate(-w / 2, -h / 2);

    const _get_d = k => [dx * k, dy * k];

    this.drawCoverFullScreenImage(bg, w, h, ..._get_d(1.0));

    const charater_draw_data = this.drawCoverFullScreenImage(charater, w, h, ..._get_d(1.2), false);
    const blur_charater = this.getBlurImage(charater, 1 / 3000);
    foot_dy = charater_foot_point * charater_draw_data[3] + charater_draw_data[1];

    this.save();
    this.translate(0, h + h - (h - foot_dy) * 2);
    this.scale(1, -1);
    this.globalAlpha *= 0.8;
    this.drawCoverFullScreenImage(blur_charater, w, h, _get_d(1.2)[0], -_get_d(1.2)[1]);
    this.restore();

    mainShaderLoader.renderToCanvas(this, "waterwave", {
        t: t,
        amplitude: 0.001,
        frequency: 100.0,
        speed: 5.0,
        starty: 0.7 + _get_d(1.2)[1] / h
    });

    this.drawCoverFullScreenImage(charater, w, h, ..._get_d(1.2));
    this.drawCoverFullScreenImage(bird, w, h, ..._get_d(1.5));

    this.restore();
};

(() => {
    const temp_cv = document.createElement("canvas");
    const temp_ctx = temp_cv.getContext("2d");

    CanvasRenderingContext2D.prototype.changeAlpha = function (alpha) {
        temp_cv.width = this.canvas.width;
        temp_cv.height = this.canvas.height;
        temp_ctx.clear();
        temp_ctx.drawImage(this.canvas, 0, 0);
        
        this.save();
        this.resetTransform();
        this.globalAlpha = alpha;
        this.clear();
        this.drawImage(temp_cv, 0, 0);
        this.restore();
    };
})();

(() => {
    const temp_cv = document.createElement("canvas");
    const temp_ctx = temp_cv.getContext("2d");

    CanvasRenderingContext2D.prototype.applyBlur = function (blur) {
        if (blur <= 0.0) return;

        const [w, h] = [this.canvas.width, this.canvas.height];
        temp_cv.width = w;
        temp_cv.height = h;
        temp_ctx.clear();
        temp_ctx.drawImage(this.canvas, 0, 0);

        const blur_px = blur * (this.canvas.width + this.canvas.height) * 2.0;
        const max_scale = Math.max(
            (w + blur_px) / w,
            (h + blur_px) / h
        );
        
        this.save();
        this.filter = `blur(${blur_px / 2.0}px)`;
        this.clear();
        this.translate(w / 2, h / 2);
        this.scale(max_scale, max_scale);
        this.translate(-w / 2, -h / 2);
        this.drawImage(temp_cv, 0, 0);
        this.restore();
    };
})();

CanvasRenderingContext2D.prototype.strokeTextEx = function (text, x, y, font, align, baseline, color, lineWidth) {
    this.save();
    this.textAlign = align;
    this.textBaseline = baseline;
    this.strokeStyle = color;
    this.lineWidth = lineWidth;
    this.font = font;
    this.strokeText(text, x, y);
    this.restore();
};

window.tryget_img = new Proxy({}, {
    get: (target, key) => {
        return window[key] || {};
    },
});

window.new_milcirc_img = (mask, n, innerCircRadius, perfect_color, good_color) => {
    window.milcirc_imgs ||= [];

    const imgs = [];
    const seed = Math.random();
    const _create = (i, color) => {
        const cv = document.createElement("canvas");
        const ctx = cv.getContext("2d", { alpha: true });
        cv.width = cv.height = 256;
        ctx.drawImage(mask, 0, 0, cv.width, cv.height);
        mainShaderLoader.renderToCanvas(ctx, "milcirc", {
            p: i / (n - 1),
            seed: seed,
            innerCircRadius: innerCircRadius,
            color: color.map(x => x / 255),
            __enableAlpha: true
        });
        return cv;
    };

    for (let i = 0; i < n; i++) {
        imgs.push([
            _create(i, perfect_color),
            _create(i, good_color)
        ]);
    }
    window.milcirc_imgs.push(imgs);
};

CanvasRenderingContext2D.prototype.drawTextOverflowEllipsis = function (text, x, y, font, align, baseline, color, maxwidth, ellipse="...") {
    this.save();
    this.font = font;
    this.textAlign = align;
    this.textBaseline = baseline;
    this.fillStyle = color;
    let cliped = false;

    while (true) {
        const width = this.getTextSize(text, font)[0];
        if (width <= maxwidth) break;
        cliped = true;
        text = text.slice(0, text.length - 1);
    }

    if (cliped) text += ellipse;
    this.fillText(text, x, y);
    this.restore();
};

CanvasRenderingContext2D.prototype.applyDim = function (im, dim) {
    if (!im.width || !im.height) return {};
    const cv = document.createElement("canvas");
    const ctx = cv.getContext("2d");
    cv.width = im.width;
    cv.height = im.height;
    ctx.drawImage(im, 0, 0);
    ctx.fillRectEx(0, 0, cv.width, cv.height, `rgba(0, 0, 0, ${dim})`);
    return cv;
};

CanvasRenderingContext2D.prototype.strokeRoundRectEx = function (x, y, w, h, r, color, lw) {
    this.save();
    this.beginPath();
    this.strokeStyle = color;
    this.lineWidth = lw;
    this.roundRect(x, y, w, h, r);
    this.stroke();
    this.restore();
};

(() => {
    const cv = document.createElement("canvas");
    const ctx = cv.getContext("2d");

    CanvasRenderingContext2D.prototype.blurRect = function (x, y, w, h, r) {
        x = parseInt(x);
        y = parseInt(y);
        w = parseInt(w);
        h = parseInt(h);
        cv.width = w;
        cv.height = h;

        ctx.clear();
        ctx.filter = `blur(${r}px)`;
        ctx.drawImage(this.canvas, -x, -y);
        this.clearRect(x, y, w, h);
        this.drawImage(cv, x, y);
    }
})();

CanvasRenderingContext2D.prototype.drawGrdTBText = function (text, x, y, font, align, baseline, topcolor, bottomcolor) {
    this.save();
    this.font = font;
    this.textAlign = align;
    this.textBaseline = baseline;
    const size = this.getTextSize(text, font);
    const grd = this.createLinearGradient(0, y, 0, y + size[1]);
    grd.addColorStop(0, topcolor);
    grd.addColorStop(1, bottomcolor);
    this.fillStyle = grd;
    this.fillText(text, x, y);
    this.restore();
};

CanvasRenderingContext2D.prototype.pathPolygon = function (points) {
    this.moveTo(points[0][0], points[0][1]);
    for (let i = 1; i < points.length; i++) {
        this.lineTo(points[i][0], points[i][1]);
    }
}

CanvasRenderingContext2D.prototype.fillPolygon = function (points, color) {
    this.save();
    this.beginPath();
    this.pathPolygon(points);
    this.fillStyle = color;
    this.fill();
    this.restore();
}

CanvasRenderingContext2D.prototype.strokeCircle = function (x, y, width, r, color) {
    this.save();
    this.beginPath();
    this.arc(x, y, r, 0, Math.PI * 2);
    this.strokeStyle = color;
    this.lineWidth = width;
    this.stroke();
    this.restore();
}

CanvasRenderingContext2D.prototype.fillCircle = function (x, y, r, color) {
    this.save();
    this.beginPath();
    this.arc(x, y, r, 0, Math.PI * 2);
    this.fillStyle = color;
    this.fill();
    this.restore();
}

CanvasRenderingContext2D.prototype.pathCircleWithClip = function (x, y, r) {
    this.arc(x, y, r, 0, Math.PI * 2);
    this.clip();
}

CanvasRenderingContext2D.prototype.createH264Codec = async function (bitrate, framerate) {
    const cfg = {
        codec: "avc1.640032",
        width: this.canvas.width,
        height: this.canvas.height,
        bitrate: bitrate,
        framerate: framerate,
        latencyMode: "realtime",
        bitrateMode: "variable",
        avc: { format: "annexb" }
    };

    const output = (chunk, meta) => {
        encoder.encoded = { chunk, meta };
    };

    const encoder = new VideoEncoder({
        output: output,
        error: err => console.error(err),
    });

    encoder.configure(cfg);
    encoder.frame_index = 0;
    encoder.framerate = framerate;
    return encoder;
}

CanvasRenderingContext2D.prototype.toH264 = async function (codec) {
    const frame = new VideoFrame(this.canvas, {
        timestamp: parseInt(codec.frame_index * 1e6 / codec.framerate),
    });

    codec.encode(frame, { keyFrame: codec.frame_index % Math.max(Math.ceil(codec.framerate / 4), 1) === 0 });
    codec.frame_index++;
    frame.close();
    await codec.flush();
    const chunk = codec.encoded.chunk;
    const buf = new Uint8Array(chunk.byteLength);
    chunk.copyTo(buf);
    return [buf, chunk.timestamp];
}

CanvasRenderingContext2D.prototype.h264EncodePromisesMerge = async function (pms) {
    const chunks = await Promise.all(pms);
    const total = chunks.reduce((a, [c, _]) => a + c.length, chunks.length * 12);
    const out = new Uint8Array(total);
    const view = new DataView(out.buffer);
    let offset = 0;

    for (let i = 0; i < chunks.length; i++) {
        const [h264_chunk, timestamp] = chunks[i];
        view.setUint32(offset, h264_chunk.length, true);
        view.setBigUint64(offset + 4, BigInt(timestamp), true);
        out.set(h264_chunk, offset + 12);
        offset += h264_chunk.length + 12;
    }
    return out;
};
