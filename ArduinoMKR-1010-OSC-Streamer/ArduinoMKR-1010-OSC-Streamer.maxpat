{
    "patcher": {
        "fileversion": 1,
        "appversion": {
            "major": 9,
            "minor": 1,
            "revision": 1,
            "architecture": "x64",
            "modernui": 1
        },
        "classnamespace": "box",
        "rect": [ 34.0, 557.0, 1572.0, 543.0 ],
        "boxes": [
            {
                "box": {
                    "fontsize": 16.44132444132444,
                    "id": "obj-70",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 539.773054420948, 211.0, 162.0, 25.0 ],
                    "presentation_linecount": 2,
                    "text": "smoothed -100…100"
                }
            },
            {
                "box": {
                    "fontsize": 16.44132444132444,
                    "id": "obj-69",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 303.77305442094803, 212.5, 120.0, 25.0 ],
                    "presentation_linecount": 2,
                    "text": "smoothed 0…1"
                }
            },
            {
                "box": {
                    "fontsize": 16.44132444132444,
                    "id": "obj-68",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 62.99999976158142, 212.5, 74.0, 25.0 ],
                    "text": "RAW out"
                }
            },
            {
                "box": {
                    "fontsize": 16.44132444132444,
                    "id": "obj-67",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 824.0, 92.0, 97.0, 25.0 ],
                    "text": "Timestamps"
                }
            },
            {
                "box": {
                    "fontsize": 16.44132444132444,
                    "id": "obj-60",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 1097.0, 96.0, 103.0, 25.0 ],
                    "text": "Digital inputs"
                }
            },
            {
                "box": {
                    "fontsize": 16.44132444132444,
                    "id": "obj-59",
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 51.0, 96.0, 108.0, 25.0 ],
                    "text": "Analog inputs"
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-51",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 704.7730548977852, 437.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-52",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 673.7730548977852, 411.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-53",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 641.7730548977852, 386.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-54",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 609.7730548977852, 360.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-55",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 578.7730548977852, 333.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-56",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 547.7730548977852, 306.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-57",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 514.7730548977852, 284.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-58",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 8,
                    "outlettype": [ "", "", "", "", "", "", "", "" ],
                    "patching_rect": [ 514.7730548977852, 237.0, 240.0, 22.0 ],
                    "text": "spat5.osc.route /A0 /A1 /A2 /A3 /A4 /A5 /A6"
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-7",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 452.7730548977852, 437.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-9",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 422.7730548977852, 411.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-10",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 391.7730548977852, 386.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-11",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 357.7730548977852, 360.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-12",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 327.7730548977852, 333.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-21",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 296.7730548977852, 306.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-40",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 262.7730548977852, 284.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-42",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 8,
                    "outlettype": [ "", "", "", "", "", "", "", "" ],
                    "patching_rect": [ 262.7730548977852, 237.0, 240.0, 22.0 ],
                    "text": "spat5.osc.route /A0 /A1 /A2 /A3 /A4 /A5 /A6"
                }
            },
            {
                "box": {
                    "id": "obj-5",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 4,
                    "outlettype": [ "", "", "", "" ],
                    "patching_rect": [ 11.0, 123.0, 774.3191646933557, 22.0 ],
                    "text": "spat5.osc.route /analog /analog-one /analog-hundred"
                }
            },
            {
                "box": {
                    "fontsize": 16.44132444132444,
                    "id": "obj-4",
                    "linecount": 2,
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [ 113.0, 22.0, 479.0, 43.0 ],
                    "text": "INSTALL Ircam's Spat5 library or change to CNMAT's OSCroute.\nBoth are available for free"
                }
            },
            {
                "box": {
                    "id": "obj-44",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 862.0, 214.0, 145.0, 22.0 ],
                    "text": "13:26:17"
                }
            },
            {
                "box": {
                    "id": "obj-43",
                    "maxclass": "message",
                    "numinlets": 2,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 780.0, 182.0, 145.0, 22.0 ],
                    "text": "00:02:20:126"
                }
            },
            {
                "box": {
                    "id": "obj-41",
                    "maxclass": "button",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 33.0, 50.0, 24.0, 24.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-34",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 1481.0, 214.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-35",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 1449.0, 214.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-36",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 1417.0, 214.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-37",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 1385.0, 214.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-38",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 1354.0, 214.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-39",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 1321.0, 214.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-32",
                    "maxclass": "number",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 824.0, 154.0, 123.41771990060806, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-23",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 1290.0, 214.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-24",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 1258.0, 214.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-25",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 1226.0, 214.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-26",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 1195.0, 214.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-27",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 1163.0, 214.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-28",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 1132.0, 214.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-29",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 1100.0, 214.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-30",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 14,
                    "outlettype": [ "", "", "", "", "", "", "", "", "", "", "", "", "", "" ],
                    "patching_rect": [ 1100.0, 167.0, 430.39239966869343, 22.0 ],
                    "text": "spat5.osc.route /D2 /D3 /D4 /D5 /D6 /D7 /D7 /D8 /D9 /D10 /D11 /D13 /D14"
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-19",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 201.0, 437.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-18",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 171.0, 408.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-17",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 137.0, 383.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-16",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 106.0, 357.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-15",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 75.0, 330.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-14",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 44.0, 303.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "format": 6,
                    "id": "obj-13",
                    "maxclass": "flonum",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "bang" ],
                    "parameter_enable": 0,
                    "patching_rect": [ 11.0, 281.0, 49.99999952316284, 22.0 ]
                }
            },
            {
                "box": {
                    "id": "obj-8",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 8,
                    "outlettype": [ "", "", "", "", "", "", "", "" ],
                    "patching_rect": [ 11.0, 237.0, 240.0, 22.0 ],
                    "text": "spat5.osc.route /A0 /A1 /A2 /A3 /A4 /A5 /A6"
                }
            },
            {
                "box": {
                    "id": "obj-6",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 4,
                    "outlettype": [ "", "", "", "" ],
                    "patching_rect": [ 824.0, 123.0, 265.0, 22.0 ],
                    "text": "spat5.osc.route /timestamp /rtimestamp /clock"
                }
            },
            {
                "box": {
                    "id": "obj-2",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 2,
                    "outlettype": [ "", "" ],
                    "patching_rect": [ 1100.0, 123.0, 129.0, 22.0 ],
                    "text": "spat5.osc.route /digital"
                }
            },
            {
                "box": {
                    "id": "obj-1",
                    "maxclass": "newobj",
                    "numinlets": 1,
                    "numoutlets": 1,
                    "outlettype": [ "" ],
                    "patching_rect": [ 11.0, 22.0, 97.0, 22.0 ],
                    "text": "udpreceive 3033"
                }
            }
        ],
        "lines": [
            {
                "patchline": {
                    "destination": [ "obj-41", 0 ],
                    "order": 0,
                    "source": [ "obj-1", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-5", 0 ],
                    "order": 1,
                    "source": [ "obj-1", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-30", 0 ],
                    "source": [ "obj-2", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-23", 0 ],
                    "source": [ "obj-30", 6 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-24", 0 ],
                    "source": [ "obj-30", 5 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-25", 0 ],
                    "source": [ "obj-30", 4 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-26", 0 ],
                    "source": [ "obj-30", 3 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-27", 0 ],
                    "source": [ "obj-30", 2 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-28", 0 ],
                    "source": [ "obj-30", 1 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-29", 0 ],
                    "source": [ "obj-30", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-34", 0 ],
                    "source": [ "obj-30", 12 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-35", 0 ],
                    "source": [ "obj-30", 11 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-36", 0 ],
                    "source": [ "obj-30", 10 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-37", 0 ],
                    "source": [ "obj-30", 9 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-38", 0 ],
                    "source": [ "obj-30", 8 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-39", 0 ],
                    "source": [ "obj-30", 7 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-10", 0 ],
                    "source": [ "obj-42", 4 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-11", 0 ],
                    "source": [ "obj-42", 3 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-12", 0 ],
                    "source": [ "obj-42", 2 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-21", 0 ],
                    "source": [ "obj-42", 1 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-40", 0 ],
                    "source": [ "obj-42", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-7", 0 ],
                    "source": [ "obj-42", 6 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-9", 0 ],
                    "source": [ "obj-42", 5 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-42", 0 ],
                    "source": [ "obj-5", 1 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-58", 0 ],
                    "source": [ "obj-5", 2 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-6", 0 ],
                    "source": [ "obj-5", 3 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-8", 0 ],
                    "source": [ "obj-5", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-51", 0 ],
                    "source": [ "obj-58", 6 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-52", 0 ],
                    "source": [ "obj-58", 5 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-53", 0 ],
                    "source": [ "obj-58", 4 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-54", 0 ],
                    "source": [ "obj-58", 3 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-55", 0 ],
                    "source": [ "obj-58", 2 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-56", 0 ],
                    "source": [ "obj-58", 1 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-57", 0 ],
                    "source": [ "obj-58", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-2", 0 ],
                    "source": [ "obj-6", 3 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-32", 0 ],
                    "source": [ "obj-6", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-43", 1 ],
                    "source": [ "obj-6", 1 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-44", 1 ],
                    "source": [ "obj-6", 2 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-13", 0 ],
                    "source": [ "obj-8", 0 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-14", 0 ],
                    "source": [ "obj-8", 1 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-15", 0 ],
                    "source": [ "obj-8", 2 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-16", 0 ],
                    "source": [ "obj-8", 3 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-17", 0 ],
                    "source": [ "obj-8", 4 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-18", 0 ],
                    "source": [ "obj-8", 5 ]
                }
            },
            {
                "patchline": {
                    "destination": [ "obj-19", 0 ],
                    "source": [ "obj-8", 6 ]
                }
            }
        ],
        "autosave": 0
    }
}