<?php

use App\Http\Controllers\ResultsController;
use App\Http\Middleware\DeviceAccess;
use Illuminate\Support\Facades\Route;

Route::get('/results', ResultsController::class)->middleware(DeviceAccess::class);
